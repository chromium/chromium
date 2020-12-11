// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/font_warmup_win.h"

#include <dwrite.h>
#include <stdint.h>
#include <map>

#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_byteorder.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/proxy_lock.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

namespace content {

namespace {

// The Skia font manager, used for the life of the process (leaked at the end).
SkFontMgr* g_warmup_fontmgr = nullptr;

// These are from ntddk.h
#if !defined(STATUS_ACCESS_DENIED)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#endif

typedef LONG NTSTATUS;

const uintptr_t kFakeSCMHandle = 0xdead0001;
const uintptr_t kFakeServiceHandle = 0xdead0002;

SC_HANDLE WINAPI OpenSCManagerWPatch(const wchar_t* machine_name,
                                     const wchar_t* database_name,
                                     DWORD access_mask) {
  ::SetLastError(0);
  return reinterpret_cast<SC_HANDLE>(kFakeSCMHandle);
}

SC_HANDLE WINAPI OpenServiceWPatch(SC_HANDLE sc_manager,
                                   const wchar_t* service_name,
                                   DWORD access_mask) {
  ::SetLastError(0);
  return reinterpret_cast<SC_HANDLE>(kFakeServiceHandle);
}

BOOL WINAPI CloseServiceHandlePatch(SC_HANDLE service_handle) {
  if (service_handle != reinterpret_cast<SC_HANDLE>(kFakeServiceHandle) &&
      service_handle != reinterpret_cast<SC_HANDLE>(kFakeSCMHandle))
    CHECK(false);
  ::SetLastError(0);
  return TRUE;
}

BOOL WINAPI StartServiceWPatch(SC_HANDLE service,
                               DWORD args,
                               const wchar_t** arg_vectors) {
  if (service != reinterpret_cast<SC_HANDLE>(kFakeServiceHandle))
    CHECK(false);
  ::SetLastError(ERROR_ACCESS_DENIED);
  return FALSE;
}

NTSTATUS WINAPI NtALpcConnectPortPatch(HANDLE* port_handle,
                                       void* port_name,
                                       void* object_attribs,
                                       void* port_attribs,
                                       DWORD flags,
                                       void* server_sid,
                                       void* message,
                                       DWORD* buffer_length,
                                       void* out_message_attributes,
                                       void* in_message_attributes,
                                       void* time_out) {
  return STATUS_ACCESS_DENIED;
}

// Class to fake out a DC or a Font object. Maintains a reference to a
// SkTypeFace to emulate the simple operation of a DC and Font.
class FakeGdiObject : public base::RefCountedThreadSafe<FakeGdiObject> {
 public:
  FakeGdiObject(uint32_t magic, void* handle)
      : handle_(handle), magic_(magic) {}

  void set_typeface(sk_sp<SkTypeface> typeface) {
    typeface_ = std::move(typeface);
  }

  sk_sp<SkTypeface> typeface() { return typeface_; }
  void* handle() { return handle_; }
  uint32_t magic() { return magic_; }

 private:
  friend class base::RefCountedThreadSafe<FakeGdiObject>;
  ~FakeGdiObject() {}

  void* handle_;
  uint32_t magic_;
  sk_sp<SkTypeface> typeface_;

  DISALLOW_COPY_AND_ASSIGN(FakeGdiObject);
};

// This class acts as a factory for creating new fake GDI objects. It also maps
// the new instances of the FakeGdiObject class to an incrementing handle value
// which is passed to the caller of the emulated GDI function for later
// reference.  We can't be sure that this won't be used in a multi-threaded
// environment so we need to ensure a lock is taken before accessing the map of
// issued objects.
class FakeGdiObjectFactory {
 public:
  FakeGdiObjectFactory() : curr_handle_(0) {}

  // Find a corresponding fake GDI object and verify its magic value.
  // The returned value is either nullptr or the validated object.
  scoped_refptr<FakeGdiObject> Validate(void* obj, uint32_t magic) {
    if (obj) {
      base::AutoLock scoped_lock(objects_lock_);
      auto handle_entry = objects_.find(obj);
      if (handle_entry != objects_.end() &&
          handle_entry->second->magic() == magic) {
        return handle_entry->second;
      }
    }
    return nullptr;
  }

  scoped_refptr<FakeGdiObject> Create(uint32_t magic) {
    base::AutoLock scoped_lock(objects_lock_);
    curr_handle_++;
    // We don't support wrapping the fake handle value.
    void* handle = reinterpret_cast<void*>(
        static_cast<uintptr_t>(curr_handle_.ValueOrDie()));
    scoped_refptr<FakeGdiObject> object(new FakeGdiObject(magic, handle));
    objects_[handle] = object;
    return object;
  }

  bool DeleteObject(void* obj, uint32_t magic) {
    base::AutoLock scoped_lock(objects_lock_);
    auto handle_entry = objects_.find(obj);
    if (handle_entry != objects_.end() &&
        handle_entry->second->magic() == magic) {
      objects_.erase(handle_entry);
      return true;
    }
    return false;
  }

  size_t GetObjectCount() {
    base::AutoLock scoped_lock(objects_lock_);
    return objects_.size();
  }

  void ResetObjectHandles() {
    base::AutoLock scoped_lock(objects_lock_);
    curr_handle_ = 0;
    objects_.clear();
  }

 private:
  base::CheckedNumeric<uintptr_t> curr_handle_;
  std::map<void*, scoped_refptr<FakeGdiObject>> objects_;
  base::Lock objects_lock_;

  DISALLOW_COPY_AND_ASSIGN(FakeGdiObjectFactory);
};

base::LazyInstance<FakeGdiObjectFactory>::Leaky g_fake_gdi_object_factory =
    LAZY_INSTANCE_INITIALIZER;

// Magic values for the fake GDI objects.
const uint32_t kFakeDCMagic = 'fkdc';
const uint32_t kFakeFontMagic = 'fkft';

sk_sp<SkTypeface> GetTypefaceFromLOGFONT(const LOGFONTW* log_font) {
  CHECK(g_warmup_fontmgr);
  int weight = log_font->lfWeight;
  if (weight == FW_DONTCARE)
    weight = SkFontStyle::kNormal_Weight;

  SkFontStyle style(weight, log_font->lfWidth,
                    log_font->lfItalic ? SkFontStyle::kItalic_Slant
                                       : SkFontStyle::kUpright_Slant);

  std::string family_name = base::WideToUTF8(log_font->lfFaceName);
#if BUILDFLAG(ENABLE_PLUGINS)
  ppapi::ProxyAutoLock lock;  // Needed for DirectWrite font proxy.
#endif                        // BUILDFLAG(ENABLE_PLUGINS)
  return sk_sp<SkTypeface>(
      g_warmup_fontmgr->matchFamilyStyle(family_name.c_str(), style));
}

HDC WINAPI CreateCompatibleDCPatch(HDC dc_handle) {
  scoped_refptr<FakeGdiObject> ret =
      g_fake_gdi_object_factory.Get().Create(kFakeDCMagic);
  return static_cast<HDC>(ret->handle());
}

HFONT WINAPI CreateFontIndirectWPatch(const LOGFONTW* log_font) {
  if (!log_font)
    return nullptr;

  sk_sp<SkTypeface> typeface = GetTypefaceFromLOGFONT(log_font);
  if (!typeface)
    return nullptr;

  scoped_refptr<FakeGdiObject> ret =
      g_fake_gdi_object_factory.Get().Create(kFakeFontMagic);
  ret->set_typeface(std::move(typeface));

  return static_cast<HFONT>(ret->handle());
}

BOOL WINAPI DeleteDCPatch(HDC dc_handle) {
  return g_fake_gdi_object_factory.Get().DeleteObject(dc_handle, kFakeDCMagic);
}

BOOL WINAPI DeleteObjectPatch(HGDIOBJ object_handle) {
  return g_fake_gdi_object_factory.Get().DeleteObject(object_handle,
                                                      kFakeFontMagic);
}

int WINAPI EnumFontFamiliesExWPatch(HDC dc_handle,
                                    LPLOGFONTW log_font,
                                    FONTENUMPROCW enum_callback,
                                    LPARAM callback_param,
                                    DWORD flags) {
  scoped_refptr<FakeGdiObject> dc_obj =
      g_fake_gdi_object_factory.Get().Validate(dc_handle, kFakeDCMagic);
  if (!dc_obj)
    return 1;

  if (!log_font || !enum_callback)
    return 1;

  sk_sp<SkTypeface> typeface = GetTypefaceFromLOGFONT(log_font);
  if (!typeface)
    return 1;

  ENUMLOGFONTEXDVW enum_log_font = {};
  enum_log_font.elfEnumLogfontEx.elfLogFont = *log_font;
  // TODO: Fill in the rest of the text metric structure. Not yet needed for
  // Flash support but might be in the future.
  NEWTEXTMETRICEXW text_metric = {};
  text_metric.ntmTm.ntmFlags = NTM_PS_OPENTYPE;

  return enum_callback(&enum_log_font.elfEnumLogfontEx.elfLogFont,
                       reinterpret_cast<TEXTMETRIC*>(&text_metric),
                       TRUETYPE_FONTTYPE, callback_param);
}

DWORD WINAPI GetFontDataPatch(HDC dc_handle,
                              DWORD table_tag,
                              DWORD table_offset,
                              LPVOID buffer,
                              DWORD buffer_length) {
  scoped_refptr<FakeGdiObject> dc_obj =
      g_fake_gdi_object_factory.Get().Validate(dc_handle, kFakeDCMagic);
  if (!dc_obj)
    return GDI_ERROR;

  sk_sp<SkTypeface> typeface = dc_obj->typeface();
  if (!typeface)
    return GDI_ERROR;

  // |getTableData| handles |buffer| being nullptr. However if it is nullptr
  // then set the size to INT32_MAX otherwise |getTableData| will return the
  // minimum value between the table entry size and the size passed in. The
  // common Windows idiom is to pass 0 as |buffer_length| when passing nullptr,
  // which would in this case result in |getTableData| returning 0 which isn't
  // the correct answer for emulating GDI. |table_tag| must also have its
  // byte order swapped to counter the swap which occurs in the called method.
  size_t length = typeface->getTableData(
      base::ByteSwap(base::strict_cast<uint32_t>(table_tag)), table_offset,
      buffer ? buffer_length : INT32_MAX, buffer);
  // We can't distinguish between an empty table and an error.
  if (length == 0)
    return GDI_ERROR;

  return base::checked_cast<DWORD>(length);
}

HGDIOBJ WINAPI SelectObjectPatch(HDC dc_handle, HGDIOBJ object_handle) {
  scoped_refptr<FakeGdiObject> dc_obj =
      g_fake_gdi_object_factory.Get().Validate(dc_handle, kFakeDCMagic);
  if (!dc_obj)
    return nullptr;

  scoped_refptr<FakeGdiObject> font_obj =
      g_fake_gdi_object_factory.Get().Validate(object_handle, kFakeFontMagic);
  if (!font_obj)
    return nullptr;

  // Construct a new fake font object to handle the old font if there's one.
  scoped_refptr<FakeGdiObject> new_font_obj;
  sk_sp<SkTypeface> old_typeface = dc_obj->typeface();
  if (old_typeface) {
    new_font_obj = g_fake_gdi_object_factory.Get().Create(kFakeFontMagic);
    new_font_obj->set_typeface(std::move(old_typeface));
  }
  dc_obj->set_typeface(font_obj->typeface());

  if (new_font_obj)
    return static_cast<HGDIOBJ>(new_font_obj->handle());
  return nullptr;
}

void DoSingleGdiPatch(base::win::IATPatchFunction& patch,
                      const base::FilePath& path,
                      const char* function_name,
                      void* new_function) {
  patch.Patch(path.value().c_str(), "gdi32.dll", function_name, new_function);
}

class GdiFontPatchDataImpl : public content::GdiFontPatchData {
 public:
  GdiFontPatchDataImpl(const base::FilePath& path);

 private:
  base::win::IATPatchFunction create_compatible_dc_patch_;
  base::win::IATPatchFunction create_font_indirect_patch_;
  base::win::IATPatchFunction create_delete_dc_patch_;
  base::win::IATPatchFunction create_delete_object_patch_;
  base::win::IATPatchFunction create_enum_font_families_patch_;
  base::win::IATPatchFunction create_get_font_data_patch_;
  base::win::IATPatchFunction create_select_object_patch_;
};

GdiFontPatchDataImpl::GdiFontPatchDataImpl(const base::FilePath& path) {
  DoSingleGdiPatch(create_compatible_dc_patch_, path, "CreateCompatibleDC",
                   reinterpret_cast<void*>(CreateCompatibleDCPatch));
  DoSingleGdiPatch(create_font_indirect_patch_, path, "CreateFontIndirectW",
                   reinterpret_cast<void*>(CreateFontIndirectWPatch));
  DoSingleGdiPatch(create_delete_dc_patch_, path, "DeleteDC",
                   reinterpret_cast<void*>(DeleteDCPatch));
  DoSingleGdiPatch(create_delete_object_patch_, path, "DeleteObject",
                   reinterpret_cast<void*>(DeleteObjectPatch));
  DoSingleGdiPatch(create_enum_font_families_patch_, path,
                   "EnumFontFamiliesExW",
                   reinterpret_cast<void*>(EnumFontFamiliesExWPatch));
  DoSingleGdiPatch(create_get_font_data_patch_, path, "GetFontData",
                   reinterpret_cast<void*>(GetFontDataPatch));
  DoSingleGdiPatch(create_select_object_patch_, path, "SelectObject",
                   reinterpret_cast<void*>(SelectObjectPatch));
}

}  // namespace

// Directwrite connects to the font cache service to retrieve information about
// fonts installed on the system etc. This works well outside the sandbox and
// within the sandbox as long as the lpc connection maintained by the current
// process with the font cache service remains valid. It appears that there
// are cases when this connection is dropped after which directwrite is unable
// to connect to the font cache service which causes problems with characters
// disappearing.
// Directwrite has fallback code to enumerate fonts if it is unable to connect
// to the font cache service. We need to intercept the following APIs to
// ensure that it does not connect to the font cache service.
// NtALpcConnectPort
// OpenSCManagerW
// OpenServiceW
// StartServiceW
// CloseServiceHandle.
// These are all IAT patched.
void PatchServiceManagerCalls() {
  static bool is_patched = false;
  if (is_patched)
    return;
  const char* service_provider_dll =
      (base::win::GetVersion() >= base::win::Version::WIN8
           ? "api-ms-win-service-management-l1-1-0.dll"
           : "advapi32.dll");

  is_patched = true;

  static base::NoDestructor<base::win::IATPatchFunction> patch_open_sc_manager;
  DWORD patched = patch_open_sc_manager->Patch(
      L"dwrite.dll", service_provider_dll, "OpenSCManagerW",
      reinterpret_cast<void*>(OpenSCManagerWPatch));
  DCHECK(patched == 0);

  static base::NoDestructor<base::win::IATPatchFunction>
      patch_close_service_handle;
  patched = patch_close_service_handle->Patch(
      L"dwrite.dll", service_provider_dll, "CloseServiceHandle",
      reinterpret_cast<void*>(CloseServiceHandlePatch));
  DCHECK(patched == 0);

  static base::NoDestructor<base::win::IATPatchFunction> patch_open_service;
  patched = patch_open_service->Patch(
      L"dwrite.dll", service_provider_dll, "OpenServiceW",
      reinterpret_cast<void*>(OpenServiceWPatch));
  DCHECK(patched == 0);

  static base::NoDestructor<base::win::IATPatchFunction> patch_start_service;
  patched = patch_start_service->Patch(
      L"dwrite.dll", service_provider_dll, "StartServiceW",
      reinterpret_cast<void*>(StartServiceWPatch));
  DCHECK(patched == 0);

  static base::NoDestructor<base::win::IATPatchFunction> patch_nt_connect_port;
  patched = patch_nt_connect_port->Patch(
      L"dwrite.dll", "ntdll.dll", "NtAlpcConnectPort",
      reinterpret_cast<void*>(NtALpcConnectPortPatch));
  DCHECK(patched == 0);
}

GdiFontPatchData* PatchGdiFontEnumeration(const base::FilePath& path) {
  if (!g_warmup_fontmgr)
    g_warmup_fontmgr = SkFontMgr_New_DirectWrite().release();
  DCHECK(g_warmup_fontmgr);
  return new GdiFontPatchDataImpl(path);
}

size_t GetEmulatedGdiHandleCountForTesting() {
  return g_fake_gdi_object_factory.Get().GetObjectCount();
}

void ResetEmulatedGdiHandlesForTesting() {
  g_fake_gdi_object_factory.Get().ResetObjectHandles();
}

void SetPreSandboxWarmupFontMgrForTesting(sk_sp<SkFontMgr> fontmgr) {
  SkSafeUnref(g_warmup_fontmgr);
  g_warmup_fontmgr = fontmgr.release();
}

}  // namespace content
