// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_
#define PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/media_stream_video_track_shared.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/socket_option_data.h"

struct PP_NetAddress_Private;

namespace ppapi {

class HostResource;
class PPB_X509Certificate_Fields;

namespace proxy {

struct PPBURLLoader_UpdateProgress_Params;
struct SerializedDirEntry;
struct SerializedFontDescription;
class SerializedHandle;
class SerializedVar;

}  // namespace proxy
}  // namespace ppapi

namespace IPC {

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<PP_Bool> {
  typedef PP_Bool param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct PPAPI_PROXY_EXPORT ParamTraits<PP_NetAddress_Private> {
  typedef PP_NetAddress_Private param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<
    ppapi::proxy::PPBURLLoader_UpdateProgress_Params> {
  typedef ppapi::proxy::PPBURLLoader_UpdateProgress_Params param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::proxy::SerializedDirEntry> {
  typedef ppapi::proxy::SerializedDirEntry param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::proxy::SerializedFontDescription> {
  typedef ppapi::proxy::SerializedFontDescription param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::proxy::SerializedHandle> {
  typedef ppapi::proxy::SerializedHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::HostResource> {
  typedef ppapi::HostResource param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::proxy::SerializedVar> {
  typedef ppapi::proxy::SerializedVar param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<
    std::vector<ppapi::proxy::SerializedVar> > {
  typedef std::vector<ppapi::proxy::SerializedVar> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::PpapiPermissions> {
  typedef ppapi::PpapiPermissions param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if !BUILDFLAG(IS_NACL) && !defined(NACL_WIN64)
template <>
struct ParamTraits<ppapi::PepperFilePath> {
  typedef ppapi::PepperFilePath param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

#endif  // !BUILDFLAG(IS_NACL) && !defined(NACL_WIN64)

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::PPB_X509Certificate_Fields> {
  typedef ppapi::PPB_X509Certificate_Fields param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct PPAPI_PROXY_EXPORT ParamTraits<ppapi::SocketOptionData> {
  typedef ppapi::SocketOptionData param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_
