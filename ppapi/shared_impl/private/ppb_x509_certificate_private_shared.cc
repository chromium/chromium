// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"

#include <utility>

#include "base/logging.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

PPB_X509Certificate_Fields::PPB_X509Certificate_Fields() {}

PPB_X509Certificate_Fields::PPB_X509Certificate_Fields(
    const PPB_X509Certificate_Fields& fields) {
  std::unique_ptr<base::ListValue> new_values(fields.values_.DeepCopy());
  values_.Swap(new_values.get());
}

void PPB_X509Certificate_Fields::SetField(
    PP_X509Certificate_Private_Field field,
    std::unique_ptr<base::Value> value) {
  uint32_t index = static_cast<uint32_t>(field);
  bool success = values_.Set(index, std::move(value));
  DCHECK(success);
}

PP_Var PPB_X509Certificate_Fields::GetFieldAsPPVar(
    PP_X509Certificate_Private_Field field) const {
  uint32_t index = static_cast<uint32_t>(field);
  const base::Value* value;
  bool success = values_.Get(index, &value);
  if (!success) {
    // Our list received might be smaller than the number of fields, so just
    // return null if the index is OOB.
    return PP_MakeNull();
  }

  switch (value->type()) {
    case base::Value::Type::NONE:
      return PP_MakeNull();
    case base::Value::Type::BOOLEAN: {
      bool val;
      value->GetAsBoolean(&val);
      return PP_MakeBool(PP_FromBool(val));
    }
    case base::Value::Type::INTEGER: {
      int val;
      value->GetAsInteger(&val);
      return PP_MakeInt32(val);
    }
    case base::Value::Type::DOUBLE: {
      double val;
      value->GetAsDouble(&val);
      return PP_MakeDouble(val);
    }
    case base::Value::Type::STRING: {
      std::string val;
      value->GetAsString(&val);
      return StringVar::StringToPPVar(val);
    }
    case base::Value::Type::BINARY: {
      uint32_t size = static_cast<uint32_t>(value->GetBlob().size());
      const uint8_t* buffer = value->GetBlob().data();
      PP_Var array_buffer =
          PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(size,
                                                                     buffer);
      return array_buffer;
    }
    case base::Value::Type::DICTIONARY:
    case base::Value::Type::LIST:
    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD:
      // Not handled.
      break;
  }

  // Should not reach here.
  CHECK(false);
  return PP_MakeUndefined();
}

//------------------------------------------------------------------------------

PPB_X509Certificate_Private_Shared::PPB_X509Certificate_Private_Shared(
    ResourceObjectType type,
    PP_Instance instance)
    : Resource(type, instance) {}

PPB_X509Certificate_Private_Shared::PPB_X509Certificate_Private_Shared(
    ResourceObjectType type,
    PP_Instance instance,
    const PPB_X509Certificate_Fields& fields)
    : Resource(type, instance),
      fields_(new PPB_X509Certificate_Fields(fields)) {
}

PPB_X509Certificate_Private_Shared::~PPB_X509Certificate_Private_Shared() {
}

thunk::PPB_X509Certificate_Private_API*
PPB_X509Certificate_Private_Shared::AsPPB_X509Certificate_Private_API() {
  return this;
}

PP_Bool PPB_X509Certificate_Private_Shared::Initialize(const char* bytes,
                                                       uint32_t length) {
  // The certificate should be immutable once initialized.
  if (fields_.get())
    return PP_FALSE;

  if (!bytes || length == 0)
    return PP_FALSE;

  std::vector<char> der(bytes, bytes + length);
  std::unique_ptr<PPB_X509Certificate_Fields> fields(
      new PPB_X509Certificate_Fields());
  bool success = ParseDER(der, fields.get());
  if (success) {
    fields_.swap(fields);
    return PP_TRUE;
  }
  return PP_FALSE;
}

PP_Var PPB_X509Certificate_Private_Shared::GetField(
    PP_X509Certificate_Private_Field field) {
  if (!fields_.get())
    return PP_MakeUndefined();

  return fields_->GetFieldAsPPVar(field);
}

bool PPB_X509Certificate_Private_Shared::ParseDER(
    const std::vector<char>& der,
    PPB_X509Certificate_Fields* result) {
  // A concrete PPB_X509Certificate_Private_Shared should only ever be
  // constructed by passing in PPB_X509Certificate_Fields, in which case it is
  // already initialized.
  CHECK(false);
  return false;
}

}  // namespace ppapi
