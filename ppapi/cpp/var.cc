// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/var.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

// Define equivalent to snprintf on Windows.
#if defined(_MSC_VER)
#  define snprintf sprintf_s
#endif

namespace pp {

namespace {

template <> const char* interface_name<PPB_Var_1_2>() {
  return PPB_VAR_INTERFACE_1_2;
}
template <> const char* interface_name<PPB_Var_1_1>() {
  return PPB_VAR_INTERFACE_1_1;
}
template <> const char* interface_name<PPB_Var_1_0>() {
  return PPB_VAR_INTERFACE_1_0;
}

// Technically you can call AddRef and Release on any Var, but it may involve
// cross-process calls depending on the plugin. This is an optimization so we
// only do refcounting on the necessary objects.
inline bool NeedsRefcounting(const PP_Var& var) {
  return var.type > PP_VARTYPE_DOUBLE;
}

// This helper function uses the latest available version of VarFromUtf8. Note
// that version 1.0 of this method has a different API to later versions.
PP_Var VarFromUtf8Helper(const char* utf8_str, uint32_t len) {
  if (has_interface<PPB_Var_1_2>()) {
    return get_interface<PPB_Var_1_2>()->VarFromUtf8(utf8_str, len);
  } else if (has_interface<PPB_Var_1_1>()) {
    return get_interface<PPB_Var_1_1>()->VarFromUtf8(utf8_str, len);
  } else if (has_interface<PPB_Var_1_0>()) {
    return get_interface<PPB_Var_1_0>()->VarFromUtf8(Module::Get()->pp_module(),
                                                     utf8_str,
                                                     len);
  }
  return PP_MakeNull();
}

// This helper function uses the latest available version of AddRef.
// Returns true on success, false if no appropriate interface was available.
bool AddRefHelper(const PP_Var& var) {
  if (has_interface<PPB_Var_1_2>()) {
    get_interface<PPB_Var_1_2>()->AddRef(var);
    return true;
  } else if (has_interface<PPB_Var_1_1>()) {
    get_interface<PPB_Var_1_1>()->AddRef(var);
    return true;
  } else if (has_interface<PPB_Var_1_0>()) {
    get_interface<PPB_Var_1_0>()->AddRef(var);
    return true;
  }
  return false;
}

// This helper function uses the latest available version of Release.
// Returns true on success, false if no appropriate interface was available.
bool ReleaseHelper(const PP_Var& var) {
  if (has_interface<PPB_Var_1_2>()) {
    get_interface<PPB_Var_1_2>()->Release(var);
    return true;
  } else if (has_interface<PPB_Var_1_1>()) {
    get_interface<PPB_Var_1_1>()->Release(var);
    return true;
  } else if (has_interface<PPB_Var_1_0>()) {
    get_interface<PPB_Var_1_0>()->Release(var);
    return true;
  }
  return false;
}

}  // namespace

Var::Var() {
  memset(&var_, 0, sizeof(var_));
  var_.type = PP_VARTYPE_UNDEFINED;
  is_managed_ = true;
}

Var::Var(Null) {
  memset(&var_, 0, sizeof(var_));
  var_.type = PP_VARTYPE_NULL;
  is_managed_ = true;
}

Var::Var(bool b) {
  var_.type = PP_VARTYPE_BOOL;
  var_.padding = 0;
  var_.value.as_bool = PP_FromBool(b);
  is_managed_ = true;
}

Var::Var(int32_t i) {
  var_.type = PP_VARTYPE_INT32;
  var_.padding = 0;
  var_.value.as_int = i;
  is_managed_ = true;
}

Var::Var(double d) {
  var_.type = PP_VARTYPE_DOUBLE;
  var_.padding = 0;
  var_.value.as_double = d;
  is_managed_ = true;
}

Var::Var(const char* utf8_str) {
  uint32_t len = utf8_str ? static_cast<uint32_t>(strlen(utf8_str)) : 0;
  var_ = VarFromUtf8Helper(utf8_str, len);
  is_managed_ = true;
}

Var::Var(const std::string& utf8_str) {
  var_ = VarFromUtf8Helper(utf8_str.c_str(),
                           static_cast<uint32_t>(utf8_str.size()));
  is_managed_ = true;
}

Var::Var(const pp::Resource& resource) {
  if (has_interface<PPB_Var_1_2>()) {
    var_ = get_interface<PPB_Var_1_2>()->VarFromResource(
        resource.pp_resource());
  } else {
    PP_NOTREACHED();
    return;
  }
  // Set |is_managed_| to true, so |var_| will be properly released upon
  // destruction.
  is_managed_ = true;
}


Var::Var(const PP_Var& var) {
  var_ = var;
  is_managed_ = true;
  if (NeedsRefcounting(var_)) {
    if (!AddRefHelper(var_))
      var_.type = PP_VARTYPE_NULL;
  }
}

Var::Var(const Var& other) {
  var_ = other.var_;
  is_managed_ = true;
  if (NeedsRefcounting(var_)) {
    if (!AddRefHelper(var_))
      var_.type = PP_VARTYPE_NULL;
  }
}

Var::~Var() {
  if (NeedsRefcounting(var_) && is_managed_)
    ReleaseHelper(var_);
}

Var& Var::operator=(const Var& other) {
  // Early return for self-assignment. Note however, that two distinct vars
  // can refer to the same object, so we still need to be careful about the
  // refcounting below.
  if (this == &other)
    return *this;

  // Be careful to keep the ref alive for cases where we're assigning an
  // object to itself by addrefing the new one before releasing the old one.
  bool old_is_managed = is_managed_;
  is_managed_ = true;
  if (NeedsRefcounting(other.var_)) {
    AddRefHelper(other.var_);
  }
  if (NeedsRefcounting(var_) && old_is_managed)
    ReleaseHelper(var_);

  var_ = other.var_;
  return *this;
}

bool Var::operator==(const Var& other) const {
  if (var_.type != other.var_.type)
    return false;
  switch (var_.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      return true;
    case PP_VARTYPE_BOOL:
      return AsBool() == other.AsBool();
    case PP_VARTYPE_INT32:
      return AsInt() == other.AsInt();
    case PP_VARTYPE_DOUBLE:
      return AsDouble() == other.AsDouble();
    case PP_VARTYPE_STRING:
      if (var_.value.as_id == other.var_.value.as_id)
        return true;
      return AsString() == other.AsString();
    case PP_VARTYPE_OBJECT:
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_ARRAY_BUFFER:
    case PP_VARTYPE_DICTIONARY:
    case PP_VARTYPE_RESOURCE:
    default:  // Objects, arrays, dictionaries, resources.
      return var_.value.as_id == other.var_.value.as_id;
  }
}

bool Var::AsBool() const {
  if (!is_bool()) {
    PP_NOTREACHED();
    return false;
  }
  return PP_ToBool(var_.value.as_bool);
}

int32_t Var::AsInt() const {
  if (is_int())
    return var_.value.as_int;
  if (is_double())
    return static_cast<int>(var_.value.as_double);
  PP_NOTREACHED();
  return 0;
}

double Var::AsDouble() const {
  if (is_double())
    return var_.value.as_double;
  if (is_int())
    return static_cast<double>(var_.value.as_int);
  PP_NOTREACHED();
  return 0.0;
}

std::string Var::AsString() const {
  if (!is_string()) {
    PP_NOTREACHED();
    return std::string();
  }

  uint32_t len;
  const char* str;
  if (has_interface<PPB_Var_1_2>())
    str = get_interface<PPB_Var_1_2>()->VarToUtf8(var_, &len);
  else if (has_interface<PPB_Var_1_1>())
    str = get_interface<PPB_Var_1_1>()->VarToUtf8(var_, &len);
  else if (has_interface<PPB_Var_1_0>())
    str = get_interface<PPB_Var_1_0>()->VarToUtf8(var_, &len);
  else
    return std::string();
  return std::string(str, len);
}

pp::Resource Var::AsResource() const {
  if (!is_resource()) {
    PP_NOTREACHED();
    return pp::Resource();
  }

  if (has_interface<PPB_Var_1_2>()) {
    return pp::Resource(pp::PASS_REF,
                        get_interface<PPB_Var_1_2>()->VarToResource(var_));
  } else {
    return pp::Resource();
  }
}

std::string Var::DebugString() const {
  char buf[256];
  if (is_undefined()) {
    snprintf(buf, sizeof(buf), "Var(UNDEFINED)");
  } else if (is_null()) {
    snprintf(buf, sizeof(buf), "Var(NULL)");
  } else if (is_bool()) {
    snprintf(buf, sizeof(buf), AsBool() ? "Var(true)" : "Var(false)");
  } else if (is_int()) {
    snprintf(buf, sizeof(buf), "Var(%d)", static_cast<int>(AsInt()));
  } else if (is_double()) {
    snprintf(buf, sizeof(buf), "Var(%f)", AsDouble());
  } else if (is_string()) {
    char format[] = "Var<'%s'>";
    size_t decoration = sizeof(format) - 2;  // The %s is removed.
    size_t available = sizeof(buf) - decoration;
    std::string str = AsString();
    if (str.length() > available) {
      str.resize(available - 3);  // Reserve space for ellipsis.
      str.append("...");
    }
    snprintf(buf, sizeof(buf), format, str.c_str());
  } else if (is_object()) {
    snprintf(buf, sizeof(buf), "Var(OBJECT)");
  } else if (is_array()) {
    snprintf(buf, sizeof(buf), "Var(ARRAY)");
  } else if (is_dictionary()) {
    snprintf(buf, sizeof(buf), "Var(DICTIONARY)");
  } else if (is_array_buffer()) {
    snprintf(buf, sizeof(buf), "Var(ARRAY_BUFFER)");
  } else if (is_resource()) {
    snprintf(buf, sizeof(buf), "Var(RESOURCE)");
  } else {
    buf[0] = '\0';
  }
  return buf;
}

}  // namespace pp
