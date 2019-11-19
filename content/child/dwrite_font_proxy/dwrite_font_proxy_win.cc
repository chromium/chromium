// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/child/dwrite_font_proxy/dwrite_localized_strings_win.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_names.mojom.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DirectWriteLoadFamilyResult {
  LOAD_FAMILY_SUCCESS_SINGLE_FAMILY = 0,
  LOAD_FAMILY_SUCCESS_MATCHED_FAMILY = 1,
  LOAD_FAMILY_ERROR_MULTIPLE_FAMILIES = 2,
  LOAD_FAMILY_ERROR_NO_FAMILIES = 3,
  LOAD_FAMILY_ERROR_NO_COLLECTION = 4,

  LOAD_FAMILY_MAX_VALUE
};

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum FontProxyError {
  FIND_FAMILY_SEND_FAILED = 0,
  GET_FAMILY_COUNT_SEND_FAILED = 1,
  COLLECTION_KEY_INVALID = 2,
  FAMILY_INDEX_OUT_OF_RANGE = 3,
  GET_FONT_FILES_SEND_FAILED = 4,
  MAPPED_FILE_FAILED = 5,
  DUPLICATE_HANDLE_FAILED = 6,

  FONT_PROXY_ERROR_MAX_VALUE
};

void LogLoadFamilyResult(DirectWriteLoadFamilyResult result) {
  UMA_HISTOGRAM_ENUMERATION("DirectWrite.Fonts.Proxy.LoadFamilyResult", result,
                            LOAD_FAMILY_MAX_VALUE);
}

void LogFamilyCount(uint32_t count) {
  UMA_HISTOGRAM_COUNTS_1000("DirectWrite.Fonts.Proxy.FamilyCount", count);
}

void LogFontProxyError(FontProxyError error) {
  UMA_HISTOGRAM_ENUMERATION("DirectWrite.Fonts.Proxy.FontProxyError", error,
                            FONT_PROXY_ERROR_MAX_VALUE);
}

}  // namespace

HRESULT DWriteFontCollectionProxy::Create(
    DWriteFontCollectionProxy** proxy_out,
    IDWriteFactory* dwrite_factory,
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy) {
  return Microsoft::WRL::MakeAndInitialize<DWriteFontCollectionProxy>(
      proxy_out, dwrite_factory, std::move(proxy));
}

DWriteFontCollectionProxy::DWriteFontCollectionProxy() = default;

DWriteFontCollectionProxy::~DWriteFontCollectionProxy() = default;

HRESULT DWriteFontCollectionProxy::FindFamilyName(const WCHAR* family_name,
                                                  UINT32* index,
                                                  BOOL* exists) {
  DCHECK(family_name);
  DCHECK(index);
  DCHECK(exists);
  TRACE_EVENT0("dwrite,fonts", "FontProxy::FindFamilyName");

  uint32_t family_index = 0;
  base::string16 name(family_name);

  auto iter = family_names_.find(name);
  if (iter != family_names_.end()) {
    *index = iter->second;
    *exists = iter->second != UINT_MAX;
    return S_OK;
  }

  if (!GetFontProxy().FindFamily(name, &family_index)) {
    LogFontProxyError(FIND_FAMILY_SEND_FAILED);
    return E_FAIL;
  }

  if (family_index != UINT32_MAX) {
    if (!CreateFamily(family_index))
      return E_FAIL;
    *exists = TRUE;
    *index = family_index;
    families_[family_index]->SetName(name);
  } else {
    *exists = FALSE;
    *index = UINT32_MAX;
  }

  family_names_[name] = *index;
  return S_OK;
}

HRESULT DWriteFontCollectionProxy::GetFontFamily(
    UINT32 index,
    IDWriteFontFamily** font_family) {
  DCHECK(font_family);

  if (index < families_.size() && families_[index]) {
    families_[index].CopyTo(font_family);
    return S_OK;
  }

  if (!CreateFamily(index))
    return E_FAIL;

  families_[index].CopyTo(font_family);
  return S_OK;
}

UINT32 DWriteFontCollectionProxy::GetFontFamilyCount() {
  if (family_count_ != UINT_MAX)
    return family_count_;

  TRACE_EVENT0("dwrite,fonts", "FontProxy::GetFontFamilyCount");

  uint32_t family_count = 0;
  if (!GetFontProxy().GetFamilyCount(&family_count)) {
    LogFontProxyError(GET_FAMILY_COUNT_SEND_FAILED);
    return 0;
  }

  LogFamilyCount(family_count);
  family_count_ = family_count;
  return family_count;
}

HRESULT DWriteFontCollectionProxy::GetFontFromFontFace(
    IDWriteFontFace* font_face,
    IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);

  for (const auto& family : families_) {
    if (family && family->GetFontFromFontFace(font_face, font)) {
      return S_OK;
    }
  }
  // If the font came from our collection, at least one family should match
  DCHECK(false);

  return E_FAIL;
}

HRESULT DWriteFontCollectionProxy::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    const void* collection_key,
    UINT32 collection_key_size,
    IDWriteFontFileEnumerator** font_file_enumerator) {
  if (!collection_key || collection_key_size != sizeof(uint32_t)) {
    LogFontProxyError(COLLECTION_KEY_INVALID);
    return E_INVALIDARG;
  }

  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadingFontFiles");

  const uint32_t* family_index =
      reinterpret_cast<const uint32_t*>(collection_key);

  if (*family_index >= GetFontFamilyCount()) {
    LogFontProxyError(FAMILY_INDEX_OUT_OF_RANGE);
    return E_INVALIDARG;
  }

  // If we already loaded the family we should reuse the existing collection.
  DCHECK(!families_[*family_index]->IsLoaded());

  std::vector<base::FilePath> file_names;
  std::vector<base::File> file_handles;
  if (!GetFontProxy().GetFontFiles(*family_index, &file_names, &file_handles)) {
    LogFontProxyError(GET_FONT_FILES_SEND_FAILED);
    return E_FAIL;
  }

  std::vector<HANDLE> handles;
  handles.reserve(file_names.size() + file_handles.size());
  for (const base::FilePath& file_name : file_names) {
    // This leaks the handles, since they are used as the reference key to
    // CreateStreamFromKey, and DirectWrite requires the reference keys to
    // remain valid for the lifetime of the loader. The loader is the font
    // collection proxy, which remains alive for the lifetime of the renderer.
    HANDLE handle =
        CreateFile(file_name.value().c_str(), GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE)
      handles.push_back(handle);
  }
  for (auto& file_handle : file_handles) {
    handles.push_back(file_handle.TakePlatformFile());
  }

  HRESULT hr = mswr::MakeAndInitialize<FontFileEnumerator>(
      font_file_enumerator, factory, this, &handles);

  if (!SUCCEEDED(hr)) {
    DCHECK(false);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT DWriteFontCollectionProxy::CreateStreamFromKey(
    const void* font_file_reference_key,
    UINT32 font_file_reference_key_size,
    IDWriteFontFileStream** font_file_stream) {
  if (font_file_reference_key_size != sizeof(HANDLE)) {
    return E_FAIL;
  }

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::CreateStreamFromKey");

  HANDLE file_handle =
      *reinterpret_cast<const HANDLE*>(font_file_reference_key);

  if (file_handle == NULL || file_handle == INVALID_HANDLE_VALUE) {
    DCHECK(false);
    return E_FAIL;
  }

  mswr::ComPtr<FontFileStream> stream;
  if (!SUCCEEDED(
          mswr::MakeAndInitialize<FontFileStream>(&stream, file_handle))) {
    DCHECK(false);
    return E_FAIL;
  }
  *font_file_stream = stream.Detach();
  return S_OK;
}

HRESULT DWriteFontCollectionProxy::RuntimeClassInitialize(
    IDWriteFactory* factory,
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy) {
  DCHECK(factory);

  factory_ = factory;
  if (proxy)
    SetProxy(std::move(proxy));
  else
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  HRESULT hr = factory->RegisterFontCollectionLoader(this);
  DCHECK(SUCCEEDED(hr));
  hr = factory_->RegisterFontFileLoader(this);
  DCHECK(SUCCEEDED(hr));
  return S_OK;
}

void DWriteFontCollectionProxy::Unregister() {
  factory_->UnregisterFontCollectionLoader(this);
  factory_->UnregisterFontFileLoader(this);
}

bool DWriteFontCollectionProxy::LoadFamily(
    UINT32 family_index,
    IDWriteFontCollection** containing_collection) {
  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadFamily");

  uint32_t index = family_index;
  // CreateCustomFontCollection ends up calling
  // DWriteFontCollectionProxy::CreateEnumeratorFromKey.
  HRESULT hr = factory_->CreateCustomFontCollection(
      this /*collectionLoader*/, reinterpret_cast<const void*>(&index),
      sizeof(index), containing_collection);

  return SUCCEEDED(hr);
}

bool DWriteFontCollectionProxy::GetFontFamily(UINT32 family_index,
                                              const base::string16& family_name,
                                              IDWriteFontFamily** font_family) {
  DCHECK(font_family);
  DCHECK(!family_name.empty());
  if (!CreateFamily(family_index))
    return false;

  mswr::ComPtr<DWriteFontFamilyProxy>& family = families_[family_index];
  if (!family->IsLoaded() || family->GetName().empty())
    family->SetName(family_name);

  family.CopyTo(font_family);
  return true;
}

bool DWriteFontCollectionProxy::LoadFamilyNames(
    UINT32 family_index,
    IDWriteLocalizedStrings** localized_strings) {
  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadFamilyNames");

  std::vector<blink::mojom::DWriteStringPairPtr> pairs;
  if (!GetFontProxy().GetFamilyNames(family_index, &pairs)) {
    return false;
  }
  std::vector<std::pair<base::string16, base::string16>> strings;
  for (auto& pair : pairs) {
    strings.emplace_back(std::move(pair->first), std::move(pair->second));
  }

  HRESULT hr = mswr::MakeAndInitialize<DWriteLocalizedStrings>(
      localized_strings, &strings);

  return SUCCEEDED(hr);
}

bool DWriteFontCollectionProxy::CreateFamily(UINT32 family_index) {
  if (family_index < families_.size() && families_[family_index])
    return true;

  UINT32 family_count = GetFontFamilyCount();
  if (family_index >= family_count) {
    return false;
  }

  if (families_.size() < family_count)
    families_.resize(family_count);

  mswr::ComPtr<DWriteFontFamilyProxy> family;
  HRESULT hr = mswr::MakeAndInitialize<DWriteFontFamilyProxy>(&family, this,
                                                              family_index);
  DCHECK(SUCCEEDED(hr));
  DCHECK_LT(family_index, families_.size());

  families_[family_index] = family;
  return true;
}

void DWriteFontCollectionProxy::SetProxy(
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy) {
  font_proxy_ = blink::mojom::ThreadSafeDWriteFontProxyPtr::Create(
      std::move(proxy),
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::WithBaseSyncPrimitives()}));
}

blink::mojom::DWriteFontProxy& DWriteFontCollectionProxy::GetFontProxy() {
  if (!font_proxy_) {
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> dwrite_font_proxy;
    if (main_task_runner_->RunsTasksInCurrentSequence()) {
      ChildThread::Get()->BindHostReceiver(
          dwrite_font_proxy.InitWithNewPipeAndPassReceiver());
    } else {
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::PendingReceiver<blink::mojom::DWriteFontProxy>
                     receiver) {
                ChildThread::Get()->BindHostReceiver(std::move(receiver));
              },
              dwrite_font_proxy.InitWithNewPipeAndPassReceiver()));
    }
    SetProxy(std::move(dwrite_font_proxy));
  }
  return **font_proxy_;
}

DWriteFontFamilyProxy::DWriteFontFamilyProxy() = default;

DWriteFontFamilyProxy::~DWriteFontFamilyProxy() = default;

HRESULT DWriteFontFamilyProxy::GetFontCollection(
    IDWriteFontCollection** font_collection) {
  DCHECK(font_collection);

  proxy_collection_.CopyTo(font_collection);
  return S_OK;
}

UINT32 DWriteFontFamilyProxy::GetFontCount() {
  // We could conceivably proxy just the font count. However, calling
  // GetFontCount is almost certain to be followed by a series of GetFont
  // calls which will need to load all the fonts anyway, so we might as
  // well save an IPC here.
  if (!LoadFamily())
    return 0;

  return family_->GetFontCount();
}

HRESULT DWriteFontFamilyProxy::GetFont(UINT32 index, IDWriteFont** font) {
  DCHECK(font);

  if (index >= GetFontCount()) {
    return E_INVALIDARG;
  }
  if (!LoadFamily())
    return E_FAIL;

  return family_->GetFont(index, font);
}

HRESULT DWriteFontFamilyProxy::GetFamilyNames(IDWriteLocalizedStrings** names) {
  DCHECK(names);

  // Prefer the real thing, if available.
  if (family_) {
    family_names_.Reset();  // Release cached data.
    return family_->GetFamilyNames(names);
  }

  // If already cached, use the cache.
  if (family_names_) {
    family_names_.CopyTo(names);
    return S_OK;
  }

  TRACE_EVENT0("dwrite,fonts", "FontProxy::GetFamilyNames");

  // Otherwise, do the IPC.
  if (!proxy_collection_->LoadFamilyNames(family_index_, &family_names_))
    return E_FAIL;

  family_names_.CopyTo(names);
  return S_OK;
}

HRESULT DWriteFontFamilyProxy::GetFirstMatchingFont(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFont** matching_font) {
  DCHECK(matching_font);

  if (!LoadFamily())
    return E_FAIL;

  return family_->GetFirstMatchingFont(weight, stretch, style, matching_font);
}

HRESULT DWriteFontFamilyProxy::GetMatchingFonts(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFontList** matching_fonts) {
  DCHECK(matching_fonts);

  if (!LoadFamily())
    return E_FAIL;

  return family_->GetMatchingFonts(weight, stretch, style, matching_fonts);
}

HRESULT DWriteFontFamilyProxy::RuntimeClassInitialize(
    DWriteFontCollectionProxy* collection,
    UINT32 index) {
  DCHECK(collection);

  proxy_collection_ = collection;
  family_index_ = index;
  return S_OK;
}

bool DWriteFontFamilyProxy::GetFontFromFontFace(IDWriteFontFace* font_face,
                                                IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);

  if (!family_)
    return false;

  mswr::ComPtr<IDWriteFontCollection> collection;
  HRESULT hr = family_->GetFontCollection(&collection);
  DCHECK(SUCCEEDED(hr));
  hr = collection->GetFontFromFontFace(font_face, font);

  return SUCCEEDED(hr);
}

void DWriteFontFamilyProxy::SetName(const base::string16& family_name) {
  family_name_.assign(family_name);
}

const base::string16& DWriteFontFamilyProxy::GetName() {
  return family_name_;
}

bool DWriteFontFamilyProxy::IsLoaded() {
  return family_ != nullptr;
}

bool DWriteFontFamilyProxy::LoadFamily() {
  if (family_)
    return true;

  SCOPED_UMA_HISTOGRAM_TIMER("DirectWrite.Fonts.Proxy.LoadFamilyTime");
  TRACE_EVENT0("dwrite,fonts", "DWriteFontFamilyProxy::LoadFamily");

  auto* font_key_name = base::debug::AllocateCrashKeyString(
      "font_key_name", base::debug::CrashKeySize::Size32);
  base::debug::ScopedCrashKeyString crash_key(font_key_name,
                                              base::WideToUTF8(family_name_));

  mswr::ComPtr<IDWriteFontCollection> collection;
  if (!proxy_collection_->LoadFamily(family_index_, &collection)) {
    LogLoadFamilyResult(LOAD_FAMILY_ERROR_NO_COLLECTION);
    return false;
  }

  UINT32 family_count = collection->GetFontFamilyCount();

  HRESULT hr;
  if (family_count > 1) {
    // Some fonts are packaged in a single file containing multiple families. In
    // such a case we can find the right family by family name.
    DCHECK(!family_name_.empty());
    UINT32 family_index = 0;
    BOOL found = FALSE;
    hr =
        collection->FindFamilyName(family_name_.c_str(), &family_index, &found);
    if (SUCCEEDED(hr) && found) {
      hr = collection->GetFontFamily(family_index, &family_);
      LogLoadFamilyResult(LOAD_FAMILY_SUCCESS_MATCHED_FAMILY);
      return SUCCEEDED(hr);
    }
  }

  DCHECK_LE(family_count, 1u);

  if (family_count == 0) {
    // This is really strange, we successfully loaded no fonts?!
    LogLoadFamilyResult(LOAD_FAMILY_ERROR_NO_FAMILIES);
    return false;
  }

  LogLoadFamilyResult(family_count == 1 ? LOAD_FAMILY_SUCCESS_SINGLE_FAMILY
                                        : LOAD_FAMILY_ERROR_MULTIPLE_FAMILIES);

  hr = collection->GetFontFamily(0, &family_);

  return SUCCEEDED(hr);
}

FontFileEnumerator::FontFileEnumerator() = default;

FontFileEnumerator::~FontFileEnumerator() = default;

HRESULT FontFileEnumerator::GetCurrentFontFile(IDWriteFontFile** file) {
  DCHECK(file);
  if (current_file_ >= files_.size()) {
    return E_FAIL;
  }

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::GetCurrentFontFile");

  // CreateCustomFontFileReference ends up calling
  // DWriteFontCollectionProxy::CreateStreamFromKey.
  HRESULT hr = factory_->CreateCustomFontFileReference(
      reinterpret_cast<const void*>(&files_[current_file_]),
      sizeof(files_[current_file_]), loader_.Get() /*IDWriteFontFileLoader*/,
      file);
  DCHECK(SUCCEEDED(hr));
  return hr;
}

HRESULT FontFileEnumerator::MoveNext(BOOL* has_current_file) {
  DCHECK(has_current_file);

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::MoveNext");
  if (next_file_ >= files_.size()) {
    *has_current_file = FALSE;
    current_file_ = UINT_MAX;
    return S_OK;
  }

  current_file_ = next_file_;
  ++next_file_;
  *has_current_file = TRUE;
  return S_OK;
}

HRESULT FontFileEnumerator::RuntimeClassInitialize(
    IDWriteFactory* factory,
    IDWriteFontFileLoader* loader,
    std::vector<HANDLE>* files) {
  factory_ = factory;
  loader_ = loader;
  files_.swap(*files);
  return S_OK;
}

FontFileStream::FontFileStream() = default;

FontFileStream::~FontFileStream() = default;

HRESULT FontFileStream::GetFileSize(UINT64* file_size) {
  *file_size = data_.length();
  return S_OK;
}

HRESULT FontFileStream::GetLastWriteTime(UINT64* last_write_time) {
  *last_write_time = 0;
  return S_OK;
}

HRESULT FontFileStream::ReadFileFragment(const void** fragment_start,
                                         UINT64 fragment_offset,
                                         UINT64 fragment_size,
                                         void** fragment_context) {
  if (fragment_offset + fragment_size < fragment_offset)
    return E_FAIL;
  if (fragment_offset + fragment_size > data_.length())
    return E_FAIL;
  *fragment_start = data_.data() + fragment_offset;
  *fragment_context = nullptr;
  return S_OK;
}

HRESULT FontFileStream::RuntimeClassInitialize(HANDLE handle) {
  // Duplicate the original handle so we can reopen the file after the memory
  // mapped section closes it.
  HANDLE duplicate_handle;
  if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                       &duplicate_handle, 0 /* dwDesiredAccess */,
                       false /* bInheritHandle */, DUPLICATE_SAME_ACCESS)) {
    LogFontProxyError(DUPLICATE_HANDLE_FAILED);
    return E_FAIL;
  }

  if (!data_.Initialize(base::File(duplicate_handle))) {
    LogFontProxyError(MAPPED_FILE_FAILED);
    return E_FAIL;
  }
  return S_OK;
}

}  // namespace content
