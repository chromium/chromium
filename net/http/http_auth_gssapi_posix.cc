// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_multi_round_parse.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "url/url_canon.h"

namespace net {

using DelegationType = HttpAuth::DelegationType;

namespace {

// Exported mechanism for GSSAPI. We always use SPNEGO:

// iso.org.dod.internet.security.mechanism.snego (1.3.6.1.5.5.2)
PaddedOidDesc CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL = {
    .desc = {6, const_cast<char*>("\x2b\x06\x01\x05\x05\x02")}};

base::span<const uint8_t> GssBufferToSpan(const gss_buffer_desc& buffer) {
  if (!buffer.value) {
    return {};
  }
  // SAFETY: The GSS-API contract is that `value` points to `length` bytes.
  return UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(buffer.value), buffer.length));
}

}  // namespace

gss_OID CHROME_GSS_SPNEGO_MECH_OID_DESC =
    &CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL.desc;

OM_uint32 DelegationTypeToFlag(DelegationType delegation_type) {
  switch (delegation_type) {
    case DelegationType::kNone:
      return 0;
    case DelegationType::kByKdcPolicy:
      return GSS_C_DELEG_POLICY_FLAG;
    case DelegationType::kUnconstrained:
      return GSS_C_DELEG_FLAG;
  }
}

// ScopedBuffer releases a gss_buffer_t when it goes out of scope.
class ScopedBuffer {
 public:
  ScopedBuffer(gss_buffer_t buffer, GSSAPILibrary* gssapi_lib)
      : buffer_(buffer), gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ScopedBuffer(const ScopedBuffer&) = delete;
  ScopedBuffer& operator=(const ScopedBuffer&) = delete;

  ~ScopedBuffer() {
    if (buffer_ != GSS_C_NO_BUFFER) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status =
          gssapi_lib_->release_buffer(&minor_status, buffer_);
      DLOG_IF(WARNING, major_status != GSS_S_COMPLETE)
          << "Problem releasing buffer. major=" << major_status
          << ", minor=" << minor_status;
      buffer_ = GSS_C_NO_BUFFER;
    }
  }

 private:
  gss_buffer_t buffer_;
  raw_ptr<GSSAPILibrary> gssapi_lib_;
};

// ScopedName releases a gss_name_t when it goes out of scope.
class ScopedName {
 public:
  ScopedName(gss_name_t name, GSSAPILibrary* gssapi_lib)
      : name_(name), gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ScopedName(const ScopedName&) = delete;
  ScopedName& operator=(const ScopedName&) = delete;

  ~ScopedName() {
    if (name_ != GSS_C_NO_NAME) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status = gssapi_lib_->release_name(&minor_status, &name_);
      if (major_status != GSS_S_COMPLETE) {
        DLOG_IF(WARNING, major_status != GSS_S_COMPLETE)
            << "Problem releasing name. "
            << GetGssStatusValue(nullptr, "gss_release_name", major_status,
                                 minor_status);
      }
      name_ = GSS_C_NO_NAME;
    }
  }

 private:
  gss_name_t name_;
  raw_ptr<GSSAPILibrary> gssapi_lib_;
};

base::DictValue GetGssStatusCodeValue(GSSAPILibrary* gssapi_lib,
                                      OM_uint32 status,
                                      OM_uint32 status_code_type) {
  base::DictValue rv;

  rv.Set("status", static_cast<int>(status));

  // Message lookups aren't performed if there's no library or if the status
  // indicates success.
  if (!gssapi_lib || status == GSS_S_COMPLETE)
    return rv;

  // gss_display_status() can potentially return multiple strings by sending
  // each string on successive invocations. State is maintained across these
  // invocations in a caller supplied OM_uint32.  After each successful call,
  // the context is set to a non-zero value that should be passed as a message
  // context to each successive gss_display_status() call.  The initial and
  // terminal values of this context storage is 0.
  OM_uint32 message_context = 0;

  // To account for the off chance that gss_display_status() misbehaves and gets
  // into an infinite loop, we'll artificially limit the number of iterations to
  // |kMaxDisplayIterations|. This limit is arbitrary.
  constexpr size_t kMaxDisplayIterations = 8;
  size_t iterations = 0;

  // In addition, each message string is again arbitrarily limited to
  // |kMaxMsgLength|. There's no real documented limit to work with here.
  constexpr size_t kMaxMsgLength = 4096;

  base::ListValue messages;
  do {
    gss_buffer_desc_struct message_buffer = GSS_C_EMPTY_BUFFER;
    ScopedBuffer message_buffer_releaser(&message_buffer, gssapi_lib);

    OM_uint32 minor_status = 0;
    OM_uint32 major_status = gssapi_lib->display_status(
        &minor_status, status, status_code_type, GSS_C_NO_OID, &message_context,
        &message_buffer);

    base::span<const uint8_t> message_bytes = GssBufferToSpan(message_buffer);
    if (major_status != GSS_S_COMPLETE || message_bytes.empty()) {
      continue;
    }

    std::string_view message_string = base::as_string_view(
        message_bytes.first(std::min(kMaxMsgLength, message_bytes.size())));

    // The returned string is almost assuredly ASCII, but be defensive.
    if (!base::IsStringUTF8(message_string))
      continue;

    messages.Append(message_string);
  } while (message_context != 0 && ++iterations < kMaxDisplayIterations);

  if (!messages.empty())
    rv.Set("message", std::move(messages));
  return rv;
}

base::DictValue GetGssStatusValue(GSSAPILibrary* gssapi_lib,
                                  std::string_view method,
                                  OM_uint32 major_status,
                                  OM_uint32 minor_status) {
  base::DictValue params;
  params.Set("function", method);
  params.Set("major_status",
             GetGssStatusCodeValue(gssapi_lib, major_status, GSS_C_GSS_CODE));
  params.Set("minor_status",
             GetGssStatusCodeValue(gssapi_lib, minor_status, GSS_C_MECH_CODE));
  return params;
}

base::DictValue OidToValue(gss_OID oid) {
  base::DictValue params;

  if (!oid || oid->length == 0) {
    params.Set("oid", "<Empty OID>");
    return params;
  }

  params.Set("length", static_cast<int>(oid->length));
  if (!oid->elements)
    return params;

  // SAFETY: The GSS-API contract is that `elements` points to `length` bytes.
  auto oid_bytes = UNSAFE_BUFFERS(
      base::span(static_cast<const char*>(oid->elements), oid->length));

  // Cap OID content at arbitrary limit 1k.
  constexpr size_t kMaxOidDataSize = 1024;
  params.Set("bytes", NetLogBinaryValue(base::as_byte_span(oid_bytes).first(
                          std::min(kMaxOidDataSize, oid_bytes.size()))));

  // Based on RFC 2744 Appendix A. Hardcoding the OIDs in the list below to
  // avoid having a static dependency on the library. The OIDs are stored as
  // bytes rather than as gss_OID_desc, see the comment on PaddedOidDesc.
  static const struct {
    const char* symbolic_name;
    base::span<const char> oid;
  } kWellKnownOIDs[] = {
      {"GSS_C_NT_USER_NAME",
       base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01")},
      {"GSS_C_NT_MACHINE_UID_NAME",
       base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x02")},
      {"GSS_C_NT_STRING_UID_NAME",
       base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x03")},
      {"GSS_C_NT_HOSTBASED_SERVICE_X",
       base::span_from_cstring("\x2b\x06\x01\x05\x06\x02")},
      {"GSS_C_NT_HOSTBASED_SERVICE",
       base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")},
      {"GSS_C_NT_ANONYMOUS",
       base::span_from_cstring("\x2b\x06\01\x05\x06\x03")},
      {"GSS_C_NT_EXPORT_NAME",
       base::span_from_cstring("\x2b\x06\x01\x05\x06\x04")}};

  for (auto& well_known_oid : kWellKnownOIDs) {
    if (well_known_oid.oid == oid_bytes) {
      params.Set("oid", well_known_oid.symbolic_name);
    }
  }

  return params;
}

base::DictValue GetDisplayNameValue(GSSAPILibrary* gssapi_lib,
                                    const gss_name_t gss_name) {
  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_buffer_desc_struct name = GSS_C_EMPTY_BUFFER;
  gss_OID name_type = GSS_C_NO_OID;

  base::DictValue rv;
  major_status =
      gssapi_lib->display_name(&minor_status, gss_name, &name, &name_type);
  ScopedBuffer scoped_output_name(&name, gssapi_lib);
  if (major_status != GSS_S_COMPLETE) {
    rv.Set("error", GetGssStatusValue(gssapi_lib, "gss_display_name",
                                      major_status, minor_status));
    return rv;
  }
  base::span<const uint8_t> name_bytes = GssBufferToSpan(name);
  std::string_view name_string = base::as_string_view(name_bytes);
  rv.Set("name", base::IsStringUTF8(name_string)
                     ? NetLogStringValue(name_string)
                     : NetLogBinaryValue(name_bytes));
  rv.Set("type", OidToValue(name_type));
  return rv;
}

base::DictValue ContextFlagsToValue(OM_uint32 flags) {
  base::DictValue rv;
  rv.Set("value", base::StringPrintf("0x%08x", flags));
  rv.Set("delegated", (flags & GSS_C_DELEG_FLAG) == GSS_C_DELEG_FLAG);
  rv.Set("mutual", (flags & GSS_C_MUTUAL_FLAG) == GSS_C_MUTUAL_FLAG);
  return rv;
}

base::DictValue GetContextStateAsValue(GSSAPILibrary* gssapi_lib,
                                       const gss_ctx_id_t context_handle) {
  base::DictValue rv;
  if (context_handle == GSS_C_NO_CONTEXT) {
    rv.Set("error", GetGssStatusValue(nullptr, "<none>", GSS_S_NO_CONTEXT, 0));
    return rv;
  }

  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_name_t src_name = GSS_C_NO_NAME;
  gss_name_t targ_name = GSS_C_NO_NAME;
  OM_uint32 lifetime_rec = 0;
  gss_OID mech_type = GSS_C_NO_OID;
  OM_uint32 ctx_flags = 0;
  int locally_initiated = 0;
  int open = 0;
  major_status = gssapi_lib->inquire_context(&minor_status,
                                             context_handle,
                                             &src_name,
                                             &targ_name,
                                             &lifetime_rec,
                                             &mech_type,
                                             &ctx_flags,
                                             &locally_initiated,
                                             &open);
  if (major_status != GSS_S_COMPLETE) {
    rv.Set("error", GetGssStatusValue(gssapi_lib, "gss_inquire_context",
                                      major_status, minor_status));
    return rv;
  }
  ScopedName scoped_src_name(src_name, gssapi_lib);
  ScopedName scoped_targ_name(targ_name, gssapi_lib);

  rv.Set("source", GetDisplayNameValue(gssapi_lib, src_name));
  rv.Set("target", GetDisplayNameValue(gssapi_lib, targ_name));
  // lifetime_rec is a uint32, while base::Value only takes ints. On 32 bit
  // platforms uint32 doesn't fit on an int.
  rv.Set("lifetime", base::NumberToString(lifetime_rec));
  rv.Set("mechanism", OidToValue(mech_type));
  rv.Set("flags", ContextFlagsToValue(ctx_flags));
  rv.Set("open", !!open);
  return rv;
}

namespace {

// Return a NetLog value for the result of loading a library.
base::DictValue LibraryLoadResultParams(const base::FilePath& library_name,
                                        std::string_view load_result) {
  base::DictValue params;
  params.Set("library_name", library_name.value());
  if (!load_result.empty())
    params.Set("load_result", load_result);
  return params;
}

}  // namespace

GSSAPISharedLibrary::GSSAPISharedLibrary(const std::string& gssapi_library_name)
    : gssapi_library_name_(gssapi_library_name) {}

GSSAPISharedLibrary::~GSSAPISharedLibrary() {
  if (gssapi_library_) {
    base::UnloadNativeLibrary(gssapi_library_);
    gssapi_library_ = nullptr;
  }
}

bool GSSAPISharedLibrary::Init(const NetLogWithSource& net_log) {
  if (!initialized_)
    InitImpl(net_log);
  return initialized_;
}

bool GSSAPISharedLibrary::InitImpl(const NetLogWithSource& net_log) {
  DCHECK(!initialized_);
  gssapi_library_ = LoadSharedLibrary(net_log);
  if (gssapi_library_ == nullptr)
    return false;
  initialized_ = true;
  return true;
}

base::NativeLibrary GSSAPISharedLibrary::LoadSharedLibrary(
    const NetLogWithSource& net_log) {
  std::vector<base::FilePath> library_names;
  if (!gssapi_library_name_.empty()) {
    library_names.emplace_back(gssapi_library_name_);
  } else {
#if BUILDFLAG(IS_APPLE)
    library_names.emplace_back("/System/Library/Frameworks/GSS.framework/GSS");
#elif BUILDFLAG(IS_OPENBSD)
    // Heimdal - OpenBSD
    library_names.emplace_back("libgssapi.so");
#else
    // MIT Kerberos - FC, Suse10, Debian
    library_names.emplace_back("libgssapi_krb5.so.2");
    // Heimdal - Suse10, MDK
    library_names.emplace_back("libgssapi.so.4");
    // Heimdal - Gentoo
    library_names.emplace_back("libgssapi.so.2");
    // Heimdal - Suse9, CITI - FC, MDK, Suse10
    library_names.emplace_back("libgssapi.so.1");
#endif
  }

  net_log.BeginEvent(NetLogEventType::AUTH_LIBRARY_LOAD);

  // There has to be at least one candidate.
  CHECK(!library_names.empty());

  base::NativeLibraryLoadError load_error;

  for (const auto& library_name : library_names) {
    load_error = base::NativeLibraryLoadError();

    // TODO(asanka): Move library loading to a separate thread.
    //               http://crbug.com/66702
    base::ScopedAllowBlocking scoped_allow_blocking_temporarily;
    base::NativeLibrary lib =
        base::LoadNativeLibrary(library_name, &load_error);
    if (lib) {
      if (BindMethods(lib, library_name.value(), net_log)) {
        net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_LOAD, [&] {
          return LibraryLoadResultParams(library_name, "");
        });
        return lib;
      }
      base::UnloadNativeLibrary(lib);
    }
  }

  // If loading failed, then log the result of the final attempt. Doing so
  // is specially important on platforms where there's only one possible
  // library. Doing so also always logs the failure when the GSSAPI library
  // name is explicitly specified.
  net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_LOAD, [&] {
    return LibraryLoadResultParams(library_names.back(), load_error.ToString());
  });
  return nullptr;
}

namespace {

base::DictValue BindFailureParams(std::string_view library_name,
                                  std::string_view method) {
  base::DictValue params;
  params.Set("library_name", library_name);
  params.Set("method", method);
  return params;
}

void* BindUntypedMethod(base::NativeLibrary lib,
                        std::string_view library_name,
                        const char* method,
                        const NetLogWithSource& net_log) {
  void* ptr = base::GetFunctionPointerFromNativeLibrary(lib, method);
  if (ptr == nullptr) {
    net_log.AddEvent(NetLogEventType::AUTH_LIBRARY_BIND_FAILED,
                     [&] { return BindFailureParams(library_name, method); });
  }
  return ptr;
}

template <typename T>
bool BindMethod(base::NativeLibrary lib,
                std::string_view library_name,
                const char* method,
                T* receiver,
                const NetLogWithSource& net_log) {
  *receiver = reinterpret_cast<T>(
      BindUntypedMethod(lib, library_name, method, net_log));
  return *receiver != nullptr;
}

}  // namespace

bool GSSAPISharedLibrary::BindMethods(base::NativeLibrary lib,
                                      std::string_view name,
                                      const NetLogWithSource& net_log) {
  bool ok = true;
  // It's unlikely for BindMethods() to fail if LoadNativeLibrary() succeeded. A
  // failure in this function indicates an interoperability issue whose
  // diagnosis requires knowing all the methods that are missing. Hence |ok| is
  // updated in a manner that prevents short-circuiting the BindGssMethod()
  // invocations.
  ok &= BindMethod(lib, name, "gss_delete_sec_context", &delete_sec_context_,
                   net_log);
  ok &= BindMethod(lib, name, "gss_display_name", &display_name_, net_log);
  ok &= BindMethod(lib, name, "gss_display_status", &display_status_, net_log);
  ok &= BindMethod(lib, name, "gss_import_name", &import_name_, net_log);
  ok &= BindMethod(lib, name, "gss_init_sec_context", &init_sec_context_,
                   net_log);
  ok &=
      BindMethod(lib, name, "gss_inquire_context", &inquire_context_, net_log);
  ok &= BindMethod(lib, name, "gss_release_buffer", &release_buffer_, net_log);
  ok &= BindMethod(lib, name, "gss_release_name", &release_name_, net_log);
  ok &=
      BindMethod(lib, name, "gss_wrap_size_limit", &wrap_size_limit_, net_log);

  if (ok) [[likely]] {
    return true;
  }

  delete_sec_context_ = nullptr;
  display_name_ = nullptr;
  display_status_ = nullptr;
  import_name_ = nullptr;
  init_sec_context_ = nullptr;
  inquire_context_ = nullptr;
  release_buffer_ = nullptr;
  release_name_ = nullptr;
  wrap_size_limit_ = nullptr;
  return false;
}

OM_uint32 GSSAPISharedLibrary::import_name(
    OM_uint32* minor_status,
    const gss_buffer_t input_name_buffer,
    const gss_OID input_name_type,
    gss_name_t* output_name) {
  DCHECK(initialized_);
  return import_name_(minor_status, input_name_buffer, input_name_type,
                      output_name);
}

OM_uint32 GSSAPISharedLibrary::release_name(
    OM_uint32* minor_status,
    gss_name_t* input_name) {
  DCHECK(initialized_);
  return release_name_(minor_status, input_name);
}

OM_uint32 GSSAPISharedLibrary::release_buffer(
    OM_uint32* minor_status,
    gss_buffer_t buffer) {
  DCHECK(initialized_);
  return release_buffer_(minor_status, buffer);
}

OM_uint32 GSSAPISharedLibrary::display_name(
    OM_uint32* minor_status,
    const gss_name_t input_name,
    gss_buffer_t output_name_buffer,
    gss_OID* output_name_type) {
  DCHECK(initialized_);
  return display_name_(minor_status,
                       input_name,
                       output_name_buffer,
                       output_name_type);
}

OM_uint32 GSSAPISharedLibrary::display_status(
    OM_uint32* minor_status,
    OM_uint32 status_value,
    int status_type,
    const gss_OID mech_type,
    OM_uint32* message_context,
    gss_buffer_t status_string) {
  DCHECK(initialized_);
  return display_status_(minor_status, status_value, status_type, mech_type,
                         message_context, status_string);
}

OM_uint32 GSSAPISharedLibrary::init_sec_context(
    OM_uint32* minor_status,
    const gss_cred_id_t initiator_cred_handle,
    gss_ctx_id_t* context_handle,
    const gss_name_t target_name,
    const gss_OID mech_type,
    OM_uint32 req_flags,
    OM_uint32 time_req,
    const gss_channel_bindings_t input_chan_bindings,
    const gss_buffer_t input_token,
    gss_OID* actual_mech_type,
    gss_buffer_t output_token,
    OM_uint32* ret_flags,
    OM_uint32* time_rec) {
  DCHECK(initialized_);
  return init_sec_context_(minor_status,
                           initiator_cred_handle,
                           context_handle,
                           target_name,
                           mech_type,
                           req_flags,
                           time_req,
                           input_chan_bindings,
                           input_token,
                           actual_mech_type,
                           output_token,
                           ret_flags,
                           time_rec);
}

OM_uint32 GSSAPISharedLibrary::wrap_size_limit(
    OM_uint32* minor_status,
    const gss_ctx_id_t context_handle,
    int conf_req_flag,
    gss_qop_t qop_req,
    OM_uint32 req_output_size,
    OM_uint32* max_input_size) {
  DCHECK(initialized_);
  return wrap_size_limit_(minor_status,
                          context_handle,
                          conf_req_flag,
                          qop_req,
                          req_output_size,
                          max_input_size);
}

OM_uint32 GSSAPISharedLibrary::delete_sec_context(
    OM_uint32* minor_status,
    gss_ctx_id_t* context_handle,
    gss_buffer_t output_token) {
  // This is called from the owner class' destructor, even if
  // Init() is not called, so we can't assume |initialized_|
  // is set.
  if (!initialized_)
    return 0;
  return delete_sec_context_(minor_status,
                             context_handle,
                             output_token);
}

OM_uint32 GSSAPISharedLibrary::inquire_context(
    OM_uint32* minor_status,
    const gss_ctx_id_t context_handle,
    gss_name_t* src_name,
    gss_name_t* targ_name,
    OM_uint32* lifetime_rec,
    gss_OID* mech_type,
    OM_uint32* ctx_flags,
    int* locally_initiated,
    int* open) {
  DCHECK(initialized_);
  return inquire_context_(minor_status,
                          context_handle,
                          src_name,
                          targ_name,
                          lifetime_rec,
                          mech_type,
                          ctx_flags,
                          locally_initiated,
                          open);
}

const std::string& GSSAPISharedLibrary::GetLibraryNameForTesting() {
  return gssapi_library_name_;
}

ScopedSecurityContext::ScopedSecurityContext(GSSAPILibrary* gssapi_lib)
    : security_context_(GSS_C_NO_CONTEXT),
      gssapi_lib_(gssapi_lib) {
  DCHECK(gssapi_lib_);
}

ScopedSecurityContext::~ScopedSecurityContext() {
  reset();
}

void ScopedSecurityContext::reset() {
  if (security_context_ != GSS_C_NO_CONTEXT) {
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    OM_uint32 minor_status = 0;
    OM_uint32 major_status = gssapi_lib_->delete_sec_context(
        &minor_status, &security_context_, &output_token);
    DLOG_IF(WARNING, major_status != GSS_S_COMPLETE)
        << "Problem releasing security_context. "
        << GetGssStatusValue(gssapi_lib_, "delete_sec_context", major_status,
                             minor_status);
    security_context_ = GSS_C_NO_CONTEXT;
  }
}

HttpAuthGSSAPI::HttpAuthGSSAPI(scoped_refptr<GSSAPILibrary> library,
                               gss_OID gss_oid)
    : gss_oid_(gss_oid),
      library_(std::move(library)),
      // Safe: `library_` is declared before `scoped_sec_context_`, so it
      // outlives the context it is borrowed by.
      scoped_sec_context_(library_.get()) {
  DCHECK(library_);
}

HttpAuthGSSAPI::~HttpAuthGSSAPI() = default;

bool HttpAuthGSSAPI::Init(const NetLogWithSource& net_log) {
  if (!library_)
    return false;
  return library_->Init(net_log);
}

bool HttpAuthGSSAPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthGSSAPI::AllowsExplicitCredentials() const {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void HttpAuthGSSAPI::SetDelegation(DelegationType delegation_type) {
  delegation_type_ = delegation_type;
}

bool HttpAuthGSSAPI::ResetSecurityContext() {
  scoped_sec_context_.reset();
  decoded_server_auth_token_.clear();
  return true;
}

namespace {

bool OidEquals(const gss_OID oid, base::span<const char> expected) {
  if (!oid || !oid->elements || oid->length == 0) {
    return false;
  }
  // SAFETY: The GSS-API contract is that `elements` points to `length` bytes.
  auto oid_bytes = UNSAFE_BUFFERS(
      base::span(static_cast<const char*>(oid->elements), oid->length));
  return oid_bytes == expected;
}

bool IsHostbasedServiceOid(const gss_OID oid) {
  // Match `GSS_C_NT_HOSTBASED_SERVICE` (1.2.840.113554.1.2.1.4) and deprecated
  // `GSS_C_NT_HOSTBASED_SERVICE_X` (1.3.6.1.5.6.2, RFC 2744 Appendix A).
  return OidEquals(oid, base::span_from_cstring(
                            "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")) ||
         OidEquals(oid, base::span_from_cstring("\x2b\x06\x01\x05\x06\x02"));
}

bool IsUserNameOid(const gss_OID oid) {
  // `GSS_C_NT_USER_NAME` (1.2.840.113554.1.2.1.1, RFC 2744 Appendix A).
  return OidEquals(
      oid, base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01"));
}

bool IsKrb5PrincipalOid(const gss_OID oid) {
  // `GSS_KRB5_NT_PRINCIPAL_NAME` (1.2.840.113554.1.2.2.1, RFC 1964 Section
  // 2.1.1).
  return OidEquals(
      oid, base::span_from_cstring("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01"));
}

bool IsSpnegoOid(const gss_OID oid) {
  // SPNEGO mechanism OID (1.3.6.1.5.5.2).
  return OidEquals(oid, base::span_from_cstring("\x2b\x06\x01\x05\x05\x02"));
}

bool IsNtlmsspOid(const gss_OID oid) {
  // Microsoft NTLMSSP mechanism OID (1.3.6.1.4.1.311.2.2.10), negotiated when
  // SPNEGO falls back from Kerberos to NTLM (e.g. `GSS.framework` on macOS or
  // `gss-ntlmssp` on Linux).
  return OidEquals(
      oid, base::span_from_cstring("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a"));
}

}  // namespace

base::expected<std::string, HttpAuthMechanism::TargetPrincipalError>
HttpAuthGSSAPI::GetTargetPrincipalHost() const {
  if (scoped_sec_context_.get() == GSS_C_NO_CONTEXT) {
    return base::unexpected(TargetPrincipalError::kNotReported);
  }
  gss_name_t targ_name = GSS_C_NO_NAME;
  gss_OID mech_type = GSS_C_NO_OID;
  int open = 0;
  OM_uint32 minor_status = 0;
  OM_uint32 major_status = library_->inquire_context(
      &minor_status, scoped_sec_context_.get(), nullptr, &targ_name, nullptr,
      &mech_type, nullptr, nullptr, &open);
  ScopedName scoped_targ_name(targ_name, library_.get());
  if (major_status != GSS_S_COMPLETE) {
    // Heimdal SPNEGO returns `GSS_S_NO_CONTEXT` for an incomplete round-1
    // context that only proposed a mechanism list (no optimistic mechanism
    // token, so no ticket or delegated credential exists yet).
    if (GSS_ROUTINE_ERROR(major_status) == GSS_S_NO_CONTEXT && !open) {
      return base::unexpected(TargetPrincipalError::kNotReported);
    }
    return base::unexpected(TargetPrincipalError::kQueryFailed);
  }
  // NTLM has no KDC tickets, TGT delegation, or krb5 DNS canonicalization, so
  // skip Kerberos principal validation once the context reports NTLMSSP. (Until
  // SPNEGO completes, MIT krb5 reports the SPNEGO OID instead, and
  // `gss-ntlmssp` displays its target as `HTTP/<host>` under
  // `GSS_C_NT_HOSTBASED_SERVICE`, which is validated below.)
  if (IsNtlmsspOid(mech_type)) {
    return base::unexpected(TargetPrincipalError::kNotReported);
  }
  // Incomplete round-1 SPNEGO contexts (and Heimdal/macOS `GSS.framework` NTLM
  // contexts) can succeed with `targ_name == GSS_C_NO_NAME`.
  if (targ_name == GSS_C_NO_NAME) {
    return base::unexpected(TargetPrincipalError::kNotReported);
  }
  gss_buffer_desc output_name_buffer = GSS_C_EMPTY_BUFFER;
  gss_OID output_name_type = GSS_C_NO_OID;
  major_status = library_->display_name(&minor_status, targ_name,
                                        &output_name_buffer, &output_name_type);
  ScopedBuffer scoped_output_name(&output_name_buffer, library_.get());
  if (major_status != GSS_S_COMPLETE || !output_name_buffer.value ||
      !output_name_buffer.length) {
    return base::unexpected(TargetPrincipalError::kQueryFailed);
  }
  std::string_view display_name(
      static_cast<const char*>(output_name_buffer.value),
      output_name_buffer.length);
  // Strip at most one trailing NUL if a GSSAPI provider includes a terminator
  // in `output_name_buffer.length`.
  if (display_name.ends_with('\0')) {
    display_name.remove_suffix(1);
  }
  if (display_name.empty()) {
    return base::unexpected(TargetPrincipalError::kQueryFailed);
  }
  // Reject interior NULs and backslash-escaped Kerberos/hostbased delimiters
  // (`\/` or `\@`), which can't be split into service and host unambiguously.
  // The explicit length keeps the embedded NUL in the search set.
  constexpr std::string_view kNulOrBackslash("\0\\", 2);
  if (display_name.find_first_of(kNulOrBackslash) != std::string_view::npos) {
    return base::unexpected(TargetPrincipalError::kUnparsableHost);
  }
  // Accept both `HTTP` and `host` service prefixes: KDCs with service aliasing
  // (e.g. FreeIPA, MIT krb5, Heimdal) can canonicalize `HTTP/<fqdn>` to the
  // machine's `host/<fqdn>@REALM` keytab principal in an authenticated
  // `TGS-REP`.
  //
  // Accept `HTTP@<host>` and `host@<host>` for hostbased name types.
  //
  // On incomplete SPNEGO contexts (`!open && IsSpnegoOid(mech_type)`, which MIT
  // krb5 reports until SPNEGO negotiation completes), also accept them under an
  // empty or unrecognized name type. This is only a defensive fallback: no
  // known provider reports that form (MIT krb5 displays `HTTP/<host>@REALM` and
  // `gss-ntlmssp` displays `HTTP/<host>`, both handled by the `http/` branch),
  // so validating the host against the URL host and policy is preferred over
  // failing authentication for an unknown provider. `GSS_C_NT_USER_NAME` and
  // `GSS_KRB5_NT_PRINCIPAL_NAME` are excluded so that 1-component Kerberos UPNs
  // such as `HTTP@REALM` are still rejected.
  std::string_view target_part;
  if (base::StartsWith(display_name, "http/",
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(display_name, "host/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    target_part = display_name.substr(5);
    size_t at_pos = target_part.find('@');
    if (at_pos != std::string_view::npos) {
      if (target_part.find('@', at_pos + 1) != std::string_view::npos) {
        return base::unexpected(TargetPrincipalError::kUnparsableHost);
      }
      target_part = target_part.substr(0, at_pos);
    }
  } else if ((IsHostbasedServiceOid(output_name_type) ||
              (!open && IsSpnegoOid(mech_type) &&
               !IsUserNameOid(output_name_type) &&
               !IsKrb5PrincipalOid(output_name_type))) &&
             (base::StartsWith(display_name, "http@",
                               base::CompareCase::INSENSITIVE_ASCII) ||
              base::StartsWith(display_name, "host@",
                               base::CompareCase::INSENSITIVE_ASCII))) {
    target_part = display_name.substr(5);
  } else {
    return base::unexpected(TargetPrincipalError::kNonHttpServicePrincipal);
  }
  if (target_part.empty() ||
      target_part.find_first_of("/@") != std::string_view::npos) {
    return base::unexpected(TargetPrincipalError::kUnparsableHost);
  }
  std::string parsed_host;
  std::string_view raw_host;
  if (!target_part.starts_with('[') &&
      target_part.find(':') != target_part.rfind(':')) {
    // Multiple colons without '[' can only be a bare IPv6 literal; skip
    // `ParseHostAndPort()`, which would treat the final `:<group>` as a port.
    raw_host = target_part;
  } else {
    int port = -1;
    if (!ParseHostAndPort(target_part, &parsed_host, &port)) {
      return base::unexpected(TargetPrincipalError::kUnparsableHost);
    }
    raw_host = parsed_host;
  }
  std::string trimmed_host = TrimEndingDot(raw_host);
  if (trimmed_host.empty() || trimmed_host.ends_with('.')) {
    return base::unexpected(TargetPrincipalError::kUnparsableHost);
  }
  url::CanonHostInfo host_info;
  std::string canonical_host =
      CanonicalizeHostSupportsBareIPV6(trimmed_host, &host_info);
  // Host canonicalization is many-to-one (for example, percent-decoding, IPv4
  // shorthand such as `0x0a.0.0.2` or `167772162`, IPv6 zero compression, and
  // UTS-46 mapping of IDN labels, fullwidth letters, or ignored code points),
  // so a distinct principal could otherwise compare equal to the requested
  // host. Require the host to already be canonical, ignoring only ASCII case
  // and IPv6 brackets.
  std::string expected_host = base::ToLowerASCII(trimmed_host);
  if (host_info.family == url::CanonHostInfo::IPV6 &&
      !expected_host.starts_with('[')) {
    expected_host = base::StrCat({"[", expected_host, "]"});
  }
  if (host_info.family == url::CanonHostInfo::BROKEN ||
      canonical_host != expected_host) {
    return base::unexpected(TargetPrincipalError::kUnparsableHost);
  }
  return canonical_host;
}

HttpAuth::AuthorizationResult HttpAuthGSSAPI::ParseChallenge(
    HttpAuthChallengeTokenizer* tok) {
  if (scoped_sec_context_.get() == GSS_C_NO_CONTEXT) {
    return ParseFirstRoundChallenge(HttpAuth::AUTH_SCHEME_NEGOTIATE, tok);
  }
  std::string encoded_auth_token;
  return ParseLaterRoundChallenge(HttpAuth::AUTH_SCHEME_NEGOTIATE, tok,
                                  &encoded_auth_token,
                                  &decoded_server_auth_token_);
}

int HttpAuthGSSAPI::GenerateAuthToken(const AuthCredentials* credentials,
                                      const std::string& spn,
                                      const std::string& channel_bindings,
                                      std::string* auth_token,
                                      const NetLogWithSource& net_log,
                                      CompletionOnceCallback /*callback*/) {
  DCHECK(auth_token);

  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  input_token.length = decoded_server_auth_token_.length();
  input_token.value = (input_token.length > 0)
                          ? const_cast<char*>(decoded_server_auth_token_.data())
                          : nullptr;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  ScopedBuffer scoped_output_token(&output_token, library_.get());
  int rv = GetNextSecurityToken(spn, channel_bindings, &input_token,
                                &output_token, net_log);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  *auth_token =
      "Negotiate " + base::Base64Encode(GssBufferToSpan(output_token));
  return OK;
}

namespace {

// GSSAPI status codes consist of a calling error (essentially, a programmer
// bug), a routine error (defined by the RFC), and supplementary information,
// all bitwise-or'ed together in different regions of the 32 bit return value.
// This means a simple switch on the return codes is not sufficient.

int MapImportNameStatusToError(OM_uint32 major_status) {
  if (major_status == GSS_S_COMPLETE)
    return OK;
  if (GSS_CALLING_ERROR(major_status) != 0)
    return ERR_UNEXPECTED;
  OM_uint32 routine_error = GSS_ROUTINE_ERROR(major_status);
  switch (routine_error) {
    case GSS_S_FAILURE:
      // Looking at the MIT Kerberos implementation, this typically is returned
      // when memory allocation fails. However, the API does not guarantee
      // that this is the case, so using ERR_UNEXPECTED rather than
      // ERR_OUT_OF_MEMORY.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_NAME:
    case GSS_S_BAD_NAMETYPE:
      return ERR_MALFORMED_IDENTITY;
    case GSS_S_DEFECTIVE_TOKEN:
      // Not mentioned in the API, but part of code.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_MECH:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

int MapInitSecContextStatusToError(OM_uint32 major_status) {
  // Although GSS_S_CONTINUE_NEEDED is an additional bit, it seems like
  // other code just checks if major_status is equivalent to it to indicate
  // that there are no other errors included.
  if (major_status == GSS_S_COMPLETE || major_status == GSS_S_CONTINUE_NEEDED)
    return OK;
  if (GSS_CALLING_ERROR(major_status) != 0)
    return ERR_UNEXPECTED;
  OM_uint32 routine_status = GSS_ROUTINE_ERROR(major_status);
  switch (routine_status) {
    case GSS_S_DEFECTIVE_TOKEN:
      return ERR_INVALID_RESPONSE;
    case GSS_S_DEFECTIVE_CREDENTIAL:
      // Not expected since this implementation uses the default credential.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_SIG:
      // Probably won't happen, but it's a bad response.
      return ERR_INVALID_RESPONSE;
    case GSS_S_NO_CRED:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case GSS_S_CREDENTIALS_EXPIRED:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case GSS_S_BAD_BINDINGS:
      // This only happens with mutual authentication.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_NO_CONTEXT:
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_NAMETYPE:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    case GSS_S_BAD_NAME:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    case GSS_S_BAD_MECH:
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_FAILURE:
      // This should be an "Unexpected Security Status" according to the
      // GSSAPI documentation, but it's typically used to indicate that
      // credentials are not correctly set up on a user machine, such
      // as a missing credential cache or hitting this after calling
      // kdestroy.
      // TODO(cbentzel): Use minor code for even better mapping?
      return ERR_MISSING_AUTH_CREDENTIALS;
    default:
      if (routine_status != 0)
        return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
      break;
  }
  OM_uint32 supplemental_status = GSS_SUPPLEMENTARY_INFO(major_status);
  // Replays could indicate an attack.
  if (supplemental_status & (GSS_S_DUPLICATE_TOKEN | GSS_S_OLD_TOKEN |
                             GSS_S_UNSEQ_TOKEN | GSS_S_GAP_TOKEN))
    return ERR_INVALID_RESPONSE;

  // At this point, every documented status has been checked.
  return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
}

base::DictValue ImportNameErrorParams(GSSAPILibrary* library,
                                      std::string_view spn,
                                      OM_uint32 major_status,
                                      OM_uint32 minor_status) {
  base::DictValue params;
  params.Set("spn", spn);
  if (major_status != GSS_S_COMPLETE)
    params.Set("status", GetGssStatusValue(library, "import_name", major_status,
                                           minor_status));
  return params;
}

base::DictValue InitSecContextErrorParams(GSSAPILibrary* library,
                                          gss_ctx_id_t context,
                                          OM_uint32 major_status,
                                          OM_uint32 minor_status) {
  base::DictValue params;
  if (major_status != GSS_S_COMPLETE)
    params.Set("status", GetGssStatusValue(library, "gss_init_sec_context",
                                           major_status, minor_status));
  if (context != GSS_C_NO_CONTEXT)
    params.Set("context", GetContextStateAsValue(library, context));
  return params;
}

}  // anonymous namespace

int HttpAuthGSSAPI::GetNextSecurityToken(const std::string& spn,
                                         const std::string& channel_bindings,
                                         gss_buffer_t in_token,
                                         gss_buffer_t out_token,
                                         const NetLogWithSource& net_log) {
  // GSSAPI header files, to this day, require OIDs passed in as non-const
  // pointers. Rather than const casting, let's just leave this as non-const.
  // Even if the OID pointer is const, the inner |elements| pointer is still
  // non-const.
  //
  // Wrap in `PaddedOidDesc` because Clang lowers even a non-static
  // `gss_OID_desc` aggregate initializer into `__DATA,__const` with a
  // 4-byte-aligned `elements` pointer on x86 macOS.
  PaddedOidDesc hostbased_service_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")}};

  // Create a name for the principal
  // TODO(cbentzel): Just do this on the first pass?
  std::string spn_principal = spn;
  gss_buffer_desc spn_buffer = GSS_C_EMPTY_BUFFER;
  spn_buffer.value = const_cast<char*>(spn_principal.c_str());
  spn_buffer.length = spn_principal.size() + 1;
  OM_uint32 minor_status = 0;
  gss_name_t principal_name = GSS_C_NO_NAME;

  OM_uint32 major_status = library_->import_name(
      &minor_status, &spn_buffer, &hostbased_service_oid.desc, &principal_name);
  net_log.AddEvent(NetLogEventType::AUTH_LIBRARY_IMPORT_NAME, [&] {
    return ImportNameErrorParams(library_.get(), spn, major_status,
                                 minor_status);
  });
  int rv = MapImportNameStatusToError(major_status);
  if (rv != OK)
    return rv;
  ScopedName scoped_name(principal_name, library_.get());

  // Continue creating a security context.
  net_log.BeginEvent(NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);
  major_status = library_->init_sec_context(
      &minor_status, GSS_C_NO_CREDENTIAL, scoped_sec_context_.receive(),
      principal_name, gss_oid_, DelegationTypeToFlag(delegation_type_),
      GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS, in_token,
      nullptr,  // actual_mech_type
      out_token,
      nullptr,  // ret flags
      nullptr);
  net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX, [&] {
    return InitSecContextErrorParams(library_.get(), scoped_sec_context_.get(),
                                     major_status, minor_status);
  });
  return MapInitSecContextStatusToError(major_status);
}

}  // namespace net
