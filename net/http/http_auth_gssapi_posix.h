// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
#define NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_

#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/native_library.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_mechanism.h"

#if BUILDFLAG(IS_APPLE)
#include <GSS/gssapi.h>
#elif BUILDFLAG(IS_FREEBSD)
#include <gssapi/gssapi.h>
#else
#include <gssapi.h>
#endif

namespace net {

class HttpAuthChallengeTokenizer;

// Mechanism OID for GSSAPI. We always use SPNEGO.
NET_EXPORT_PRIVATE extern gss_OID CHROME_GSS_SPNEGO_MECH_OID_DESC;

// GSSAPILibrary is introduced so unit tests can mock the calls to the GSSAPI
// library. The default implementation attempts to load one of the standard
// GSSAPI library implementations, then simply passes the arguments on to
// that implementation.
class NET_EXPORT_PRIVATE GSSAPILibrary {
 public:
  virtual ~GSSAPILibrary() = default;

  // Initializes the library, including any necessary dynamic libraries.
  // This is done separately from construction (which happens at startup time)
  // in order to delay work until the class is actually needed.
  virtual bool Init(const NetLogWithSource& net_log) = 0;

  // These methods match the ones in the GSSAPI library.
  virtual OM_uint32 import_name(
      OM_uint32* minor_status,
      const gss_buffer_t input_name_buffer,
      const gss_OID input_name_type,
      gss_name_t* output_name) = 0;
  virtual OM_uint32 release_name(
      OM_uint32* minor_status,
      gss_name_t* input_name) = 0;
  virtual OM_uint32 release_buffer(
      OM_uint32* minor_status,
      gss_buffer_t buffer) = 0;
  virtual OM_uint32 display_name(
      OM_uint32* minor_status,
      const gss_name_t input_name,
      gss_buffer_t output_name_buffer,
      gss_OID* output_name_type) = 0;
  virtual OM_uint32 display_status(
      OM_uint32* minor_status,
      OM_uint32 status_value,
      int status_type,
      const gss_OID mech_type,
      OM_uint32* message_contex,
      gss_buffer_t status_string) = 0;
  virtual OM_uint32 init_sec_context(
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
      OM_uint32* time_rec) = 0;
  virtual OM_uint32 wrap_size_limit(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      int conf_req_flag,
      gss_qop_t qop_req,
      OM_uint32 req_output_size,
      OM_uint32* max_input_size) = 0;
  virtual OM_uint32 delete_sec_context(
      OM_uint32* minor_status,
      gss_ctx_id_t* context_handle,
      gss_buffer_t output_token) = 0;
  virtual OM_uint32 inquire_context(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      gss_name_t* src_name,
      gss_name_t* targ_name,
      OM_uint32* lifetime_rec,
      gss_OID* mech_type,
      OM_uint32* ctx_flags,
      int* locally_initiated,
      int* open) = 0;
  virtual const std::string& GetLibraryNameForTesting() = 0;
};

// GSSAPISharedLibrary class is defined here so that unit tests can access it.
class NET_EXPORT_PRIVATE GSSAPISharedLibrary : public GSSAPILibrary {
 public:
  // If |gssapi_library_name| is empty, hard-coded default library names are
  // used.
  explicit GSSAPISharedLibrary(const std::string& gssapi_library_name);
  ~GSSAPISharedLibrary() override;

  // GSSAPILibrary methods:
  bool Init(const NetLogWithSource& net_log) override;
  OM_uint32 import_name(OM_uint32* minor_status,
                        const gss_buffer_t input_name_buffer,
                        const gss_OID input_name_type,
                        gss_name_t* output_name) override;
  OM_uint32 release_name(OM_uint32* minor_status,
                         gss_name_t* input_name) override;
  OM_uint32 release_buffer(OM_uint32* minor_status,
                           gss_buffer_t buffer) override;
  OM_uint32 display_name(OM_uint32* minor_status,
                         const gss_name_t input_name,
                         gss_buffer_t output_name_buffer,
                         gss_OID* output_name_type) override;
  OM_uint32 display_status(OM_uint32* minor_status,
                           OM_uint32 status_value,
                           int status_type,
                           const gss_OID mech_type,
                           OM_uint32* message_contex,
                           gss_buffer_t status_string) override;
  OM_uint32 init_sec_context(OM_uint32* minor_status,
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
                             OM_uint32* time_rec) override;
  OM_uint32 wrap_size_limit(OM_uint32* minor_status,
                            const gss_ctx_id_t context_handle,
                            int conf_req_flag,
                            gss_qop_t qop_req,
                            OM_uint32 req_output_size,
                            OM_uint32* max_input_size) override;
  OM_uint32 delete_sec_context(OM_uint32* minor_status,
                               gss_ctx_id_t* context_handle,
                               gss_buffer_t output_token) override;
  OM_uint32 inquire_context(OM_uint32* minor_status,
                            const gss_ctx_id_t context_handle,
                            gss_name_t* src_name,
                            gss_name_t* targ_name,
                            OM_uint32* lifetime_rec,
                            gss_OID* mech_type,
                            OM_uint32* ctx_flags,
                            int* locally_initiated,
                            int* open) override;
  const std::string& GetLibraryNameForTesting() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup);

  bool InitImpl(const NetLogWithSource& net_log);
  // Finds a usable dynamic library for GSSAPI and loads it.  The criteria are:
  //   1. The library must exist.
  //   2. The library must export the functions we need.
  base::NativeLibrary LoadSharedLibrary(const NetLogWithSource& net_log);
  bool BindMethods(base::NativeLibrary lib,
                   std::string_view library_name,
                   const NetLogWithSource& net_log);

  bool initialized_ = false;

  std::string gssapi_library_name_;
  // Need some way to invalidate the library.
  base::NativeLibrary gssapi_library_ = nullptr;

  // Function pointers
  decltype(&gss_import_name) import_name_ = nullptr;
  decltype(&gss_release_name) release_name_ = nullptr;
  decltype(&gss_release_buffer) release_buffer_ = nullptr;
  decltype(&gss_display_name) display_name_ = nullptr;
  decltype(&gss_display_status) display_status_ = nullptr;
  decltype(&gss_init_sec_context) init_sec_context_ = nullptr;
  decltype(&gss_wrap_size_limit) wrap_size_limit_ = nullptr;
  decltype(&gss_delete_sec_context) delete_sec_context_ = nullptr;
  decltype(&gss_inquire_context) inquire_context_ = nullptr;
};

// ScopedSecurityContext releases a gss_ctx_id_t when it goes out of
// scope.
class ScopedSecurityContext {
 public:
  explicit ScopedSecurityContext(GSSAPILibrary* gssapi_lib);

  ScopedSecurityContext(const ScopedSecurityContext&) = delete;
  ScopedSecurityContext& operator=(const ScopedSecurityContext&) = delete;

  ~ScopedSecurityContext();

  gss_ctx_id_t get() const { return security_context_; }
  gss_ctx_id_t* receive() { return &security_context_; }

 private:
  gss_ctx_id_t security_context_ = GSS_C_NO_CONTEXT;
  raw_ptr<GSSAPILibrary> gssapi_lib_;
};


// TODO(ahendrickson): Share code with HttpAuthSSPI.
class NET_EXPORT_PRIVATE HttpAuthGSSAPI : public HttpAuthMechanism {
 public:
  HttpAuthGSSAPI(GSSAPILibrary* library,
                 const gss_OID gss_oid);
  ~HttpAuthGSSAPI() override;

  // HttpAuthMechanism implementation:
  bool Init(const NetLogWithSource& net_log) override;
  bool NeedsIdentity() const override;
  bool AllowsExplicitCredentials() const override;
  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok) override;
  int GenerateAuthToken(const AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const NetLogWithSource& net_log,
                        CompletionOnceCallback callback) override;
  void SetDelegation(HttpAuth::DelegationType delegation_type) override;

 private:
  int GetNextSecurityToken(const std::string& spn,
                           const std::string& channel_bindings,
                           gss_buffer_t in_token,
                           gss_buffer_t out_token,
                           const NetLogWithSource& net_log);

  gss_OID gss_oid_;
  raw_ptr<GSSAPILibrary> library_;
  std::string decoded_server_auth_token_;
  ScopedSecurityContext scoped_sec_context_;
  HttpAuth::DelegationType delegation_type_ = HttpAuth::DelegationType::kNone;
};

// Diagnostics

// GetGssStatusCodeValue constructs a base::Value::Dict containing a status code
// and a message.
//
//     {
//       "status" : <status value as a number>,
//       "message": [
//          <list of strings explaining what that number means>
//       ]
//     }
//
// Messages are looked up via gss_display_status() exposed by |gssapi_lib|. The
// type of status code should be indicated by setting |status_code_type| to
// either |GSS_C_MECH_CODE| or |GSS_C_GSS_CODE|.
//
// Mechanism specific codes aren't unique, so the mechanism needs to be
// identified to look up messages if |status_code_type| is |GSS_C_MECH_CODE|.
// Since no mechanism OIDs are passed in, mechanism specific status codes will
// likely not have messages.
NET_EXPORT_PRIVATE base::Value::Dict GetGssStatusCodeValue(
    GSSAPILibrary* gssapi_lib,
    OM_uint32 status,
    OM_uint32 status_code_type);

// Given major and minor GSSAPI status codes, returns a base::Value::Dict
// encapsulating the codes as well as their meanings as expanded via
// gss_display_status().
//
// The base::Value::Dict has the following structure:
//   {
//     "function": <name of GSSAPI function that returned the error>
//     "major_status": {
//       "status" : <status value as a number>,
//       "message": [
//          <list of strings hopefully explaining what that number means>
//       ]
//     },
//     "minor_status": {
//       "status" : <status value as a number>,
//       "message": [
//          <list of strings hopefully explaining what that number means>
//       ]
//     }
//   }
//
// Passing nullptr to |gssapi_lib| will skip the message lookups. Thus the
// returned value will be missing the "message" fields. The same is true if the
// message lookup failed for some reason, or if the lookups succeeded but
// yielded an empty message.
NET_EXPORT_PRIVATE base::Value::Dict GetGssStatusValue(
    GSSAPILibrary* gssapi_lib,
    std::string_view method,
    OM_uint32 major_status,
    OM_uint32 minor_status);

// OidToValue returns a base::Value::Dict representing an OID. The structure of
// the value is:
//   {
//     "oid":    <symbolic name of OID if it is known>
//     "length": <length in bytes of serialized OID>,
//     "bytes":  <hexdump of up to 1024 bytes of serialized OID>
//   }
NET_EXPORT_PRIVATE base::Value::Dict OidToValue(const gss_OID oid);

// GetDisplayNameValue returns a base::Value::Dict representing a gss_name_t. It
// invokes |gss_display_name()| via |gssapi_lib| to determine the display name
// associated with |gss_name|.
//
// The structure of the returned value is:
//   {
//     "gss_name": <display name as returned by gss_display_name()>,
//     "type": <OID indicating type. See OidToValue() for structure of this
//              field>
//   }
//
// If the lookup failed, then the structure is:
//   {
//     "error": <error. See GetGssStatusValue() for structure.>
//   }
//
// Note that |gss_name_t| is platform dependent. If |gss_display_name| fails,
// there's no good value to display in its stead.
NET_EXPORT_PRIVATE base::Value::Dict GetDisplayNameValue(
    GSSAPILibrary* gssapi_lib,
    const gss_name_t gss_name);

// GetContextStateAsValue returns a base::Value::Dict that describes the state
// of a GSSAPI context. The structure of the value is:
//
//   {
//     "source": {
//       "name": <GSSAPI principal name of source (e.g. the user)>,
//       "type": <OID of name type>
//     },
//     "target": {
//       "name": <GSSAPI principal name of target (e.g. the server)>,
//       "type": <OID of name type>
//     },
//     "lifetime": <Lifetime of the negotiated context in seconds.>,
//     "mechanism": <OID of negotiated mechanism>,
//     "flags": <Context flags. See documentation for gss_inquire_context for
//               flag values>
//     "open": <True if the context has finished the handshake>
//   }
//
// If the inquiry fails, the following is returned:
//   {
//     "error": <error. See GetGssStatusValue() for structure.>
//   }
NET_EXPORT_PRIVATE base::Value::Dict GetContextStateAsValue(
    GSSAPILibrary* gssapi_lib,
    const gss_ctx_id_t context_handle);
}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
