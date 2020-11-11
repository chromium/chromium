// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/api_permission.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

namespace extensions {

// An abstract base class for permissions that are represented by the
// disjunction of a set of conditions.  Each condition is represented by a
// |PermissionDataType| (e.g. SocketPermissionData).  If an
// APIPermission::CheckParam matches any of the conditions in the set, the
// permission is granted.
//
// For an example of how to use this class, see SocketPermission.
template <class PermissionDataType, class DerivedType>
class SetDisjunctionPermission : public APIPermission {
 public:
  explicit SetDisjunctionPermission(const APIPermissionInfo* info)
      : APIPermission(info) {}

  ~SetDisjunctionPermission() override {}

  // APIPermission overrides
  bool Check(const APIPermission::CheckParam* param) const override {
    for (typename std::set<PermissionDataType>::const_iterator i =
             data_set_.begin();
         i != data_set_.end();
         ++i) {
      if (i->Check(param))
        return true;
    }
    return false;
  }

  bool Contains(const APIPermission* rhs) const override {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    return base::ranges::includes(data_set_, perm->data_set_);
  }

  bool Equal(const APIPermission* rhs) const override {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    return data_set_ == perm->data_set_;
  }

  std::unique_ptr<APIPermission> Clone() const override {
    auto result = std::make_unique<DerivedType>(info());
    result->data_set_ = data_set_;
    return result;
  }

  std::unique_ptr<APIPermission> Diff(const APIPermission* rhs) const override {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    std::unique_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    result->data_set_ = base::STLSetDifference<std::set<PermissionDataType> >(
        data_set_, perm->data_set_);
    return result->data_set_.empty() ? nullptr : std::move(result);
  }

  std::unique_ptr<APIPermission> Union(
      const APIPermission* rhs) const override {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    std::unique_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    result->data_set_ = base::STLSetUnion<std::set<PermissionDataType> >(
        data_set_, perm->data_set_);
    return result;
  }

  std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const override {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    std::unique_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    result->data_set_ = base::STLSetIntersection<std::set<PermissionDataType> >(
        data_set_, perm->data_set_);
    return result->data_set_.empty() ? nullptr : std::move(result);
  }

  bool FromValue(
      const base::Value* value,
      std::string* error,
      std::vector<std::string>* unhandled_permissions) override {
    data_set_.clear();
    const base::ListValue* list = NULL;

    if (!value) {
      // treat null as an empty list.
      return true;
    }

    if (!value->GetAsList(&list)) {
      if (error)
        *error = "Cannot parse the permission list. It's not a list.";
      return false;
    }

    for (size_t i = 0; i < list->GetSize(); ++i) {
      const base::Value* item_value = NULL;
      bool got_item = list->Get(i, &item_value);
      DCHECK(got_item);
      DCHECK(item_value);

      PermissionDataType data;
      if (data.FromValue(item_value)) {
        data_set_.insert(data);
      } else {
        std::string unknown_permission;
        base::JSONWriter::Write(*item_value, &unknown_permission);
        if (unhandled_permissions) {
          unhandled_permissions->push_back(unknown_permission);
        } else {
          if (error) {
            *error = "Cannot parse an item from the permission list: " +
                     unknown_permission;
          }
          return false;
        }
      }
    }
    return true;
  }

  std::unique_ptr<base::Value> ToValue() const override {
    base::ListValue* list = new base::ListValue();
    typename std::set<PermissionDataType>::const_iterator i;
    for (i = data_set_.begin(); i != data_set_.end(); ++i) {
      std::unique_ptr<base::Value> item_value(i->ToValue());
      list->Append(std::move(item_value));
    }
    return std::unique_ptr<base::Value>(list);
  }

  void Write(base::Pickle* m) const override { IPC::WriteParam(m, data_set_); }

  bool Read(const base::Pickle* m, base::PickleIterator* iter) override {
    return IPC::ReadParam(m, iter, &data_set_);
  }

  void Log(std::string* log) const override {
    IPC::LogParam(data_set_, log);
  }

 protected:
  std::set<PermissionDataType> data_set_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_
