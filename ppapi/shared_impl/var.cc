// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var.h"

#include <stddef.h>

#include <limits>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

// Var -------------------------------------------------------------------------

// static
std::string Var::PPVarToLogString(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      return "[Undefined]";
    case PP_VARTYPE_NULL:
      return "[Null]";
    case PP_VARTYPE_BOOL:
      return var.value.as_bool ? "[True]" : "[False]";
    case PP_VARTYPE_INT32:
      return base::NumberToString(var.value.as_int);
    case PP_VARTYPE_DOUBLE:
      return base::NumberToString(var.value.as_double);
    case PP_VARTYPE_STRING: {
      StringVar* string(StringVar::FromPPVar(var));
      if (!string)
        return "[Invalid string]";

      // Since this is for logging, escape NULLs, truncate length.
      std::string result;
      const size_t kTruncateAboveLength = 128;
      if (string->value().size() > kTruncateAboveLength)
        result = string->value().substr(0, kTruncateAboveLength) + "...";
      else
        result = string->value();

      base::ReplaceSubstringsAfterOffset(
          &result, 0, base::StringPiece("\0", 1), "\\0");
      return result;
    }
    case PP_VARTYPE_OBJECT:
      return "[Object]";
    case PP_VARTYPE_ARRAY:
      return "[Array]";
    case PP_VARTYPE_DICTIONARY:
      return "[Dictionary]";
    case PP_VARTYPE_ARRAY_BUFFER:
      return "[Array buffer]";
    case PP_VARTYPE_RESOURCE: {
      ResourceVar* resource(ResourceVar::FromPPVar(var));
      if (!resource)
        return "[Invalid resource]";

      if (resource->IsPending()) {
        return base::StringPrintf("[Pending resource]");
      } else if (resource->GetPPResource()) {
        return base::StringPrintf("[Resource %d]", resource->GetPPResource());
      } else {
        return "[Null resource]";
      }
    }
    default:
      return "[Invalid var]";
  }
}

StringVar* Var::AsStringVar() { return NULL; }

ArrayBufferVar* Var::AsArrayBufferVar() { return NULL; }

V8ObjectVar* Var::AsV8ObjectVar() { return NULL; }

ProxyObjectVar* Var::AsProxyObjectVar() { return NULL; }

ArrayVar* Var::AsArrayVar() { return NULL; }

DictionaryVar* Var::AsDictionaryVar() { return NULL; }

ResourceVar* Var::AsResourceVar() { return NULL; }

PP_Var Var::GetPPVar() {
  int32_t id = GetOrCreateVarID();
  if (!id)
    return PP_MakeNull();

  PP_Var result;
  result.type = GetType();
  result.padding = 0;
  result.value.as_id = id;
  return result;
}

int32_t Var::GetExistingVarID() const {
  return var_id_;
}

Var::Var() : var_id_(0) {}

Var::~Var() {}

int32_t Var::GetOrCreateVarID() {
  VarTracker* tracker = PpapiGlobals::Get()->GetVarTracker();
  if (var_id_) {
    if (!tracker->AddRefVar(var_id_))
      return 0;
  } else {
    var_id_ = tracker->AddVar(this);
    if (!var_id_)
      return 0;
  }
  return var_id_;
}

void Var::AssignVarID(int32_t id) {
  DCHECK(!var_id_);  // Must not have already been generated.
  var_id_ = id;
}

// StringVar -------------------------------------------------------------------

StringVar::StringVar() {}

StringVar::StringVar(const std::string& str) : value_(str) {}

StringVar::StringVar(const char* str, uint32_t len) : value_(str, len) {}

StringVar::~StringVar() {}

StringVar* StringVar::AsStringVar() { return this; }

PP_VarType StringVar::GetType() const { return PP_VARTYPE_STRING; }

// static
PP_Var StringVar::StringToPPVar(const std::string& var) {
  return StringToPPVar(var.c_str(), static_cast<uint32_t>(var.size()));
}

// static
PP_Var StringVar::StringToPPVar(const char* data, uint32_t len) {
  scoped_refptr<StringVar> str(new StringVar(data, len));
  if (!str.get() || !base::IsStringUTF8(str->value()))
    return PP_MakeNull();
  return str->GetPPVar();
}

// static
StringVar* StringVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_STRING)
    return NULL;
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsStringVar();
}

// static
PP_Var StringVar::SwapValidatedUTF8StringIntoPPVar(std::string* src) {
  scoped_refptr<StringVar> str(new StringVar);
  str->value_.swap(*src);
  return str->GetPPVar();
}

// ArrayBufferVar --------------------------------------------------------------

ArrayBufferVar::ArrayBufferVar() {}

ArrayBufferVar::~ArrayBufferVar() {}

ArrayBufferVar* ArrayBufferVar::AsArrayBufferVar() { return this; }

PP_VarType ArrayBufferVar::GetType() const { return PP_VARTYPE_ARRAY_BUFFER; }

// static
ArrayBufferVar* ArrayBufferVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_ARRAY_BUFFER)
    return NULL;
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsArrayBufferVar();
}

}  // namespace ppapi
