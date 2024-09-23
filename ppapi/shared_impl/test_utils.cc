// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <unordered_map>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "ipc/ipc_message.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

namespace {

// When two vars x and y are found to be equal, an entry is inserted into
// |visited_map| with (x.value.as_id, y.value.as_id). This allows reference
// cycles to be avoided. It also allows us to associate nodes in |expected| with
// nodes in |actual| and check whether the graphs have equivalent topology.
bool Equals(const PP_Var& expected,
            const PP_Var& actual,
            bool test_string_references,
            std::unordered_map<int64_t, int64_t>* visited_map) {
  if (expected.type != actual.type) {
    LOG(ERROR) << "expected type: " << expected.type
               << " actual type: " << actual.type;
    return false;
  }
  if (VarTracker::IsVarTypeRefcounted(expected.type)) {
    std::unordered_map<int64_t, int64_t>::iterator it =
        visited_map->find(expected.value.as_id);
    if (it != visited_map->end()) {
      if (it->second != actual.value.as_id) {
        LOG(ERROR) << "expected id: " << it->second
                   << " actual id: " << actual.value.as_id;
        return false;
      } else {
        return true;
      }
    } else {
      if (expected.type != PP_VARTYPE_STRING || test_string_references)
        (*visited_map)[expected.value.as_id] = actual.value.as_id;
    }
  }
  switch (expected.type) {
    case PP_VARTYPE_UNDEFINED:
      return true;
    case PP_VARTYPE_NULL:
      return true;
    case PP_VARTYPE_BOOL:
      if (expected.value.as_bool != actual.value.as_bool) {
        LOG(ERROR) << "expected: " << expected.value.as_bool
                   << " actual: " << actual.value.as_bool;
        return false;
      }
      return true;
    case PP_VARTYPE_INT32:
      if (expected.value.as_int != actual.value.as_int) {
        LOG(ERROR) << "expected: " << expected.value.as_int
                   << " actual: " << actual.value.as_int;
        return false;
      }
      return true;
    case PP_VARTYPE_DOUBLE:
      if (fabs(expected.value.as_double - actual.value.as_double) > 1.0e-4) {
        LOG(ERROR) << "expected: " << expected.value.as_double
                   << " actual: " << actual.value.as_double;
        return false;
      }
      return true;
    case PP_VARTYPE_OBJECT:
      if (expected.value.as_id != actual.value.as_id) {
        LOG(ERROR) << "expected: " << expected.value.as_id
                   << " actual: " << actual.value.as_id;
        return false;
      }
      return true;
    case PP_VARTYPE_STRING: {
      StringVar* expected_var = StringVar::FromPPVar(expected);
      StringVar* actual_var = StringVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->value() != actual_var->value()) {
        LOG(ERROR) << "expected: " << expected_var->value()
                   << " actual: " << actual_var->value();
        return false;
      }
      return true;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* expected_var = ArrayBufferVar::FromPPVar(expected);
      ArrayBufferVar* actual_var = ArrayBufferVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->ByteLength() != actual_var->ByteLength()) {
        LOG(ERROR) << "expected: " << expected_var->ByteLength()
                   << " actual: " << actual_var->ByteLength();
        return false;
      }
      if (memcmp(expected_var->Map(),
                 actual_var->Map(),
                 expected_var->ByteLength()) != 0) {
        LOG(ERROR) << "expected array buffer does not match actual.";
        return false;
      }
      return true;
    }
    case PP_VARTYPE_ARRAY: {
      ArrayVar* expected_var = ArrayVar::FromPPVar(expected);
      ArrayVar* actual_var = ArrayVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->elements().size() != actual_var->elements().size()) {
        LOG(ERROR) << "expected: " << expected_var->elements().size()
                   << " actual: " << actual_var->elements().size();
        return false;
      }
      for (size_t i = 0; i < expected_var->elements().size(); ++i) {
        if (!Equals(expected_var->elements()[i].get(),
                    actual_var->elements()[i].get(),
                    test_string_references,
                    visited_map)) {
          return false;
        }
      }
      return true;
    }
    case PP_VARTYPE_DICTIONARY: {
      DictionaryVar* expected_var = DictionaryVar::FromPPVar(expected);
      DictionaryVar* actual_var = DictionaryVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->key_value_map().size() !=
          actual_var->key_value_map().size()) {
        LOG(ERROR) << "expected: " << expected_var->key_value_map().size()
                   << " actual: " << actual_var->key_value_map().size();
        return false;
      }
      DictionaryVar::KeyValueMap::const_iterator expected_iter =
          expected_var->key_value_map().begin();
      DictionaryVar::KeyValueMap::const_iterator actual_iter =
          actual_var->key_value_map().begin();
      for (; expected_iter != expected_var->key_value_map().end();
           ++expected_iter, ++actual_iter) {
        if (expected_iter->first != actual_iter->first) {
          LOG(ERROR) << "expected: " << expected_iter->first
                     << " actual: " << actual_iter->first;
          return false;
        }
        if (!Equals(expected_iter->second.get(),
                    actual_iter->second.get(),
                    test_string_references,
                    visited_map)) {
          return false;
        }
      }
      return true;
    }
    case PP_VARTYPE_RESOURCE: {
      ResourceVar* expected_var = ResourceVar::FromPPVar(expected);
      ResourceVar* actual_var = ResourceVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->GetPPResource() != actual_var->GetPPResource()) {
        LOG(ERROR) << "expected: " << expected_var->GetPPResource()
                   << " actual: " << actual_var->GetPPResource();
        return false;
      }

      const IPC::Message* actual_message = actual_var->GetCreationMessage();
      const IPC::Message* expected_message = expected_var->GetCreationMessage();
      if (expected_message->size() != actual_message->size()) {
        LOG(ERROR) << "expected creation message size: "
                   << expected_message->size()
                   << " actual: " << actual_message->size();
        return false;
      }

      // Set the upper 24 bits of actual creation_message flags to the same as
      // expected. This is an unpredictable reference number that changes
      // between serialization/deserialization, and we do not want it to cause
      // the comparison to fail.
      IPC::Message local_actual_message(*actual_message);
      local_actual_message.SetHeaderValues(
          actual_message->routing_id(),
          actual_message->type(),
          (expected_message->flags() & 0xffffff00) |
              (actual_message->flags() & 0xff));
      if (memcmp(expected_message->data(),
                 local_actual_message.data(),
                 expected_message->size()) != 0) {
        LOG(ERROR) << "expected creation message does not match actual.";
        return false;
      }
      return true;
    }
  }
  NOTREACHED();
}

}  // namespace

bool TestEqual(const PP_Var& expected,
               const PP_Var& actual,
               bool test_string_references) {
  std::unordered_map<int64_t, int64_t> visited_map;
  return Equals(expected, actual, test_string_references, &visited_map);
}

std::string StripTestPrefixes(const std::string& test_name) {
  const char kDisabledPrefix[] = "DISABLED_";
  if (base::StartsWith(test_name, kDisabledPrefix,
                       base::CompareCase::SENSITIVE)) {
    return test_name.substr(sizeof(kDisabledPrefix) - 1);
  }
  return test_name;
}

}  // namespace ppapi
