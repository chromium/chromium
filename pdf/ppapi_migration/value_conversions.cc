// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/value_conversions.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/values.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"

namespace chrome_pdf {

pp::Var VarFromValue(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::NONE:
      return pp::Var::Null();
    case base::Value::Type::BOOLEAN:
      return pp::Var(value.GetBool());
    case base::Value::Type::INTEGER:
      return pp::Var(value.GetInt());
    case base::Value::Type::DOUBLE:
      return pp::Var(value.GetDouble());
    case base::Value::Type::STRING:
      return pp::Var(value.GetString());
    case base::Value::Type::BINARY: {
      const base::Value::BlobStorage& blob = value.GetBlob();
      pp::VarArrayBuffer buffer(blob.size());
      std::copy(blob.begin(), blob.end(), static_cast<uint8_t*>(buffer.Map()));
      return buffer;
    }
    case base::Value::Type::DICTIONARY: {
      pp::VarDictionary var_dict;
      for (const auto& value_pair : value.DictItems()) {
        var_dict.Set(value_pair.first, VarFromValue(value_pair.second));
      }
      return var_dict;
    }
    case base::Value::Type::LIST: {
      pp::VarArray var_array;
      uint32_t i = 0;
      for (const auto& val : value.GetList()) {
        var_array.Set(i, VarFromValue(val));
        i++;
      }
      return var_array;
    }
    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD:
      CHECK(false);
      return pp::Var();
  }
}

base::Value ValueFromVar(const pp::Var& var) {
  switch (var.pp_var().type) {
    case PP_VARTYPE_UNDEFINED:
      return base::Value();
    case PP_VARTYPE_NULL:
      return base::Value();
    case PP_VARTYPE_BOOL:
      return base::Value(var.AsBool());
    case PP_VARTYPE_INT32:
      return base::Value(var.AsInt());
    case PP_VARTYPE_DOUBLE:
      return base::Value(var.AsDouble());
    case PP_VARTYPE_STRING:
      return base::Value(var.AsString());
    case PP_VARTYPE_OBJECT:
      // There is no valid conversion from PP_VARTYPE_OBJECT to a base::Value
      // type. This should not be called to convert this type.
      NOTREACHED();
      return base::Value();
    case PP_VARTYPE_ARRAY: {
      pp::VarArray var_array(var);
      base::Value::ListStorage list_storage(var_array.GetLength());
      for (uint32_t i = 0; i < var_array.GetLength(); ++i) {
        list_storage[i] = ValueFromVar(var_array.Get(i));
      }
      return base::Value(std::move(list_storage));
    }
    case PP_VARTYPE_DICTIONARY: {
      base::Value val_dictionary(base::Value::Type::DICTIONARY);
      pp::VarDictionary var_dict(var);
      pp::VarArray dict_keys = var_dict.GetKeys();
      for (uint32_t i = 0; i < dict_keys.GetLength(); ++i) {
        pp::Var key = dict_keys.Get(i);
        val_dictionary.SetKey(key.AsString(), ValueFromVar(var_dict.Get(key)));
      }
      return val_dictionary;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      pp::VarArrayBuffer var_array_buffer(var);
      base::Value value(
          base::make_span(static_cast<uint8_t*>(var_array_buffer.Map()),
                          var_array_buffer.ByteLength()));
      var_array_buffer.Unmap();
      return value;
    }
    case PP_VARTYPE_RESOURCE:
      // There is no valid conversion from PP_VARTYPE_RESOURCE to a base::Value
      // type. This should not be called to convert this type.
      NOTREACHED();
      return base::Value();
  }
}

}  // namespace chrome_pdf
