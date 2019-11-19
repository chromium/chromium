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

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params()
    : location(Manifest::INVALID_LOCATION),
      creation_flags(Extension::NO_FLAGS) {}

ExtensionMsg_Loaded_Params::~ExtensionMsg_Loaded_Params() {}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const Extension* extension,
    bool include_tab_permissions)
    : manifest(static_cast<base::DictionaryValue&&>(
          extension->manifest()->value()->Clone())),
      location(extension->location()),
      path(extension->path()),
      active_permissions(extension->permissions_data()->active_permissions()),
      withheld_permissions(
          extension->permissions_data()->withheld_permissions()),
      policy_blocked_hosts(
          extension->permissions_data()->policy_blocked_hosts().Clone()),
      policy_allowed_hosts(
          extension->permissions_data()->policy_allowed_hosts().Clone()),
      uses_default_policy_blocked_allowed_hosts(
          extension->permissions_data()->UsesDefaultPolicyHostRestrictions()),
      id(extension->id()),
      creation_flags(extension->creation_flags()) {
  if (include_tab_permissions) {
    for (const auto& pair :
         extension->permissions_data()->tab_specific_permissions()) {
      tab_specific_permissions[pair.first] =
          ExtensionMsg_PermissionSetStruct(*pair.second);
    }
  }
}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    ExtensionMsg_Loaded_Params&& other) = default;
ExtensionMsg_Loaded_Params& ExtensionMsg_Loaded_Params::operator=(
    ExtensionMsg_Loaded_Params&& other) = default;

scoped_refptr<Extension> ExtensionMsg_Loaded_Params::ConvertToExtension(
    std::string* error) const {
  // We pass in the |id| to the create call because it will save work in the
  // normal case, and because in tests, extensions may not have paths or keys,
  // but it's important to retain the same id.
  scoped_refptr<Extension> extension =
      Extension::Create(path, location, manifest, creation_flags, id, error);
  if (extension.get()) {
    const extensions::PermissionsData* permissions_data =
        extension->permissions_data();
    permissions_data->SetPermissions(active_permissions.ToPermissionSet(),
                                     withheld_permissions.ToPermissionSet());
    if (uses_default_policy_blocked_allowed_hosts) {
      permissions_data->SetUsesDefaultHostRestrictions();
    } else {
      permissions_data->SetPolicyHostRestrictions(policy_blocked_hosts,
                                                  policy_allowed_hosts);
    }
    for (const auto& pair : tab_specific_permissions) {
      permissions_data->UpdateTabSpecificPermissions(
          pair.first, *pair.second.ToPermissionSet());
    }
  }
  return extension;
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

void ParamTraits<APIPermission::ID>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<APIPermission::ID>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* p) {
  int api_id = -2;
  if (!ReadParam(m, iter, &api_id))
    return false;

  *p = static_cast<APIPermission::ID>(api_id);
  return true;
}

void ParamTraits<APIPermission::ID>::Log(
    const param_type& p, std::string* l) {
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
    APIPermission::ID id;
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

void ParamTraits<HostID>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.type());
  WriteParam(m, p.id());
}

bool ParamTraits<HostID>::Read(const base::Pickle* m,
                               base::PickleIterator* iter,
                               param_type* r) {
  HostID::HostType type;
  std::string id;
  if (!ReadParam(m, iter, &type))
    return false;
  if (!ReadParam(m, iter, &id))
    return false;
  *r = HostID(type, id);
  return true;
}

void ParamTraits<HostID>::Log(
    const param_type& p, std::string* l) {
  LogParam(p.type(), l);
  LogParam(p.id(), l);
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

void ParamTraits<ExtensionMsg_Loaded_Params>::Write(base::Pickle* m,
                                                    const param_type& p) {
  WriteParam(m, p.location);
  WriteParam(m, p.path);
  WriteParam(m, p.manifest);
  WriteParam(m, p.creation_flags);
  WriteParam(m, p.id);
  WriteParam(m, p.active_permissions);
  WriteParam(m, p.withheld_permissions);
  WriteParam(m, p.tab_specific_permissions);
  WriteParam(m, p.policy_blocked_hosts);
  WriteParam(m, p.policy_allowed_hosts);
  WriteParam(m, p.uses_default_policy_blocked_allowed_hosts);
}

bool ParamTraits<ExtensionMsg_Loaded_Params>::Read(const base::Pickle* m,
                                                   base::PickleIterator* iter,
                                                   param_type* p) {
  p->manifest.Clear();
  return ReadParam(m, iter, &p->location) && ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, &p->manifest) &&
         ReadParam(m, iter, &p->creation_flags) && ReadParam(m, iter, &p->id) &&
         ReadParam(m, iter, &p->active_permissions) &&
         ReadParam(m, iter, &p->withheld_permissions) &&
         ReadParam(m, iter, &p->tab_specific_permissions) &&
         ReadParam(m, iter, &p->policy_blocked_hosts) &&
         ReadParam(m, iter, &p->policy_allowed_hosts) &&
         ReadParam(m, iter, &p->uses_default_policy_blocked_allowed_hosts);
}

void ParamTraits<ExtensionMsg_Loaded_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append(p.id);
}

}  // namespace IPC
