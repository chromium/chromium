// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_messages.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "content/public/common/common_param_traits.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"

using extensions::ActivationSequence;
using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::APIPermissionSet;
using extensions::Extension;
using extensions::Manifest;
using extensions::ManifestHandler;
using extensions::ManifestPermission;
using extensions::ManifestPermissionSet;
using extensions::PermissionSet;
using extensions::URLPatternSet;
using extensions::mojom::APIPermissionID;
using extensions::mojom::ManifestLocation;

ExtensionMsg_PermissionSetStruct::ExtensionMsg_PermissionSetStruct() {
}

ExtensionMsg_PermissionSetStruct::ExtensionMsg_PermissionSetStruct(
    const PermissionSet& permissions)
    : apis(permissions.apis().Clone()),
      manifest_permissions(permissions.manifest_permissions().Clone()),
      explicit_hosts(permissions.explicit_hosts().Clone()),
      scriptable_hosts(permissions.scriptable_hosts().Clone()) {}

ExtensionMsg_PermissionSetStruct::~ExtensionMsg_PermissionSetStruct() {}

ExtensionMsg_PermissionSetStruct::ExtensionMsg_PermissionSetStruct(
    ExtensionMsg_PermissionSetStruct&& other) = default;

ExtensionMsg_PermissionSetStruct& ExtensionMsg_PermissionSetStruct::operator=(
    ExtensionMsg_PermissionSetStruct&& other) = default;

std::unique_ptr<const PermissionSet>
ExtensionMsg_PermissionSetStruct::ToPermissionSet() const {
  // TODO(devlin): Make this destructive so we can std::move() the members.
  return std::make_unique<PermissionSet>(
      apis.Clone(), manifest_permissions.Clone(), explicit_hosts.Clone(),
      scriptable_hosts.Clone());
}

namespace IPC {

void ParamTraits<URLPattern>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.valid_schemes());
  WriteParam(m, p.GetAsString());
}

bool ParamTraits<URLPattern>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* p) {
  int valid_schemes;
  std::string spec;
  if (!ReadParam(m, iter, &valid_schemes) ||
      !ReadParam(m, iter, &spec))
    return false;

  // TODO(jstritar): We don't want the URLPattern to fail parsing when the
  // scheme is invalid. Instead, the pattern should parse but it should not
  // match the invalid patterns. We get around this by setting the valid
  // schemes after parsing the pattern. Update these method calls once we can
  // ignore scheme validation with URLPattern parse options. crbug.com/90544
  p->SetValidSchemes(URLPattern::SCHEME_ALL);
  URLPattern::ParseResult result = p->Parse(spec);
  p->SetValidSchemes(valid_schemes);
  return URLPattern::ParseResult::kSuccess == result;
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<URLPatternSet>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.patterns());
}

bool ParamTraits<URLPatternSet>::Read(const base::Pickle* m,
                                      base::PickleIterator* iter,
                                      param_type* p) {
  std::set<URLPattern> patterns;
  if (!ReadParam(m, iter, &patterns))
    return false;

  for (auto i = patterns.begin(); i != patterns.end(); ++i)
    p->AddPattern(*i);
  return true;
}

void ParamTraits<URLPatternSet>::Log(const param_type& p, std::string* l) {
  LogParam(p.patterns(), l);
}

void ParamTraits<APIPermissionID>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<APIPermissionID>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  int api_id = -2;
  if (!ReadParam(m, iter, &api_id))
    return false;

  *p = static_cast<APIPermissionID>(api_id);
  return true;
}

void ParamTraits<APIPermissionID>::Log(const param_type& p, std::string* l) {
  LogParam(static_cast<int>(p), l);
}

void ParamTraits<APIPermissionSet>::Write(base::Pickle* m,
                                          const param_type& p) {
  APIPermissionSet::const_iterator it = p.begin();
  const APIPermissionSet::const_iterator end = p.end();
  WriteParam(m, static_cast<uint32_t>(p.size()));
  for (; it != end; ++it) {
    WriteParam(m, it->id());
    it->Write(m);
  }
}

bool ParamTraits<APIPermissionSet>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* r) {
  uint32_t size;
  if (!ReadParam(m, iter, &size))
    return false;
  for (uint32_t i = 0; i < size; ++i) {
    APIPermissionID id;
    if (!ReadParam(m, iter, &id))
      return false;
    const APIPermissionInfo* permission_info =
        extensions::PermissionsInfo::GetInstance()->GetByID(id);
    if (!permission_info)
      return false;
    std::unique_ptr<APIPermission> p(permission_info->CreateAPIPermission());
    if (!p->Read(m, iter))
      return false;
    r->insert(std::move(p));
  }
  return true;
}

void ParamTraits<APIPermissionSet>::Log(
    const param_type& p, std::string* l) {
  LogParam(p.map(), l);
}

void ParamTraits<ManifestPermissionSet>::Write(base::Pickle* m,
                                               const param_type& p) {
  ManifestPermissionSet::const_iterator it = p.begin();
  const ManifestPermissionSet::const_iterator end = p.end();
  WriteParam(m, static_cast<uint32_t>(p.size()));
  for (; it != end; ++it) {
    WriteParam(m, it->name());
    it->Write(m);
  }
}

bool ParamTraits<ManifestPermissionSet>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  uint32_t size;
  if (!ReadParam(m, iter, &size))
    return false;
  for (uint32_t i = 0; i < size; ++i) {
    std::string name;
    if (!ReadParam(m, iter, &name))
      return false;
    std::unique_ptr<ManifestPermission> p(
        ManifestHandler::CreatePermission(name));
    if (!p)
      return false;
    if (!p->Read(m, iter))
      return false;
    r->insert(std::move(p));
  }
  return true;
}

void ParamTraits<ManifestPermissionSet>::Log(
    const param_type& p, std::string* l) {
  LogParam(p.map(), l);
}

void ParamTraits<ExtensionMsg_PermissionSetStruct>::Write(base::Pickle* m,
                                                          const param_type& p) {
  WriteParam(m, p.apis);
  WriteParam(m, p.manifest_permissions);
  WriteParam(m, p.explicit_hosts);
  WriteParam(m, p.scriptable_hosts);
}

bool ParamTraits<ExtensionMsg_PermissionSetStruct>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  return ReadParam(m, iter, &p->apis) &&
         ReadParam(m, iter, &p->manifest_permissions) &&
         ReadParam(m, iter, &p->explicit_hosts) &&
         ReadParam(m, iter, &p->scriptable_hosts);
}

void ParamTraits<ExtensionMsg_PermissionSetStruct>::Log(const param_type& p,
                                                        std::string* l) {
  LogParam(p.apis, l);
  LogParam(p.manifest_permissions, l);
  LogParam(p.explicit_hosts, l);
  LogParam(p.scriptable_hosts, l);
}

}  // namespace IPC
