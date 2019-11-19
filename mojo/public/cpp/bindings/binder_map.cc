// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binder_map.h"

#include "base/stl_util.h"

namespace mojo {

BinderMap::BinderMap() = default;

BinderMap::BinderMap(BinderMap&&) = default;

BinderMap::BinderMap(const BinderMap&) = default;

BinderMap::~BinderMap() = default;

BinderMap& BinderMap::operator=(BinderMap&&) = default;

BinderMap& BinderMap::operator=(const BinderMap&) = default;

bool BinderMap::Bind(GenericPendingReceiver* receiver) {
  DCHECK(receiver && *receiver) << "Attempted to bind null or invalid receiver";
  auto iter = binders_.find(*receiver->interface_name());
  if (iter == binders_.end())
    return false;

  iter->second.Run(std::move(*receiver));
  return true;
}

bool BinderMap::CanBind(const GenericPendingReceiver& receiver) const {
  DCHECK(receiver);
  return base::Contains(binders_, *receiver.interface_name());
}

void BinderMap::AddGenericBinder(base::StringPiece name, GenericBinder binder) {
  auto result = binders_.emplace(name.as_string(), std::move(binder));
  DCHECK(result.second) << "Binder already registered for " << name;
}

}  // namespace mojo
