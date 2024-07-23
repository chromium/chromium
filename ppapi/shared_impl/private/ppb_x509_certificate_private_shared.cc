// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"

#include <utility>

#include "base/check.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

PPB_X509Certificate_Fields::PPB_X509Certificate_Fields() = default;

PPB_X509Certificate_Fields::PPB_X509Certificate_Fields(
    const PPB_X509Certificate_Fields& fields)
    : values_(fields.values_.Clone()) {}

void PPB_X509Certificate_Fields::SetField(
    PP_X509Certificate_Private_Field field,
    base::Value value) {
  uint32_t index = static_cast<uint32_t>(field);
  // Pad the list with null values if necessary.
  while (index >= values_.size())
    values_.Append(base::Value());
  values_[index] = std::move(value);
}

PP_Var PPB_X509Certificate_Fields::GetFieldAsPPVar(
    PP_X509Certificate_Private_Field field) const {
  uint32_t index = static_cast<uint32_t>(field);
  if (index >= values_.size()) {
    // Our list received might be smaller than the number of fields, so just
    // return null if the index is OOB.
    return PP_MakeNull();
  }

  const base::Value& value = values_[index];
  switch (value.type()) {
    case base::Value::Type::NONE:
      return PP_MakeNull();
    case base::Value::Type::BOOLEAN: {
      return PP_MakeBool(PP_FromBool(value.GetBool()));
    }
    case base::Value::Type::INTEGER: {
      return PP_MakeInt32(value.GetInt());
    }
    case base::Value::Type::DOUBLE: {
      return PP_MakeDouble(value.GetDouble());
    }
    case base::Value::Type::STRING: {
      return StringVar::StringToPPVar(value.GetString());
    }
    case base::Value::Type::BINARY: {
      uint32_t size = static_cast<uint32_t>(value.GetBlob().size());
      const uint8_t* buffer = value.GetBlob().data();
      PP_Var array_buffer =
          PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(size,
                                                                     buffer);
      return array_buffer;
    }
    case base::Value::Type::DICT:
    case base::Value::Type::LIST:
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
