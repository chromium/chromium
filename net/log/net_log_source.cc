// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

namespace {

base::Value SourceEventParametersCallback(const NetLogSource source) {
  if (!source.IsValid())
    return base::Value();
  base::DictionaryValue event_params;
  source.AddToEventParameters(&event_params);
  return std::move(event_params);
}

}  // namespace

// LoadTimingInfo requires this be 0.
const uint32_t NetLogSource::kInvalidId = 0;

NetLogSource::NetLogSource() : type(NetLogSourceType::NONE), id(kInvalidId) {}

NetLogSource::NetLogSource(NetLogSourceType type, uint32_t id)
    : type(type), id(id) {}

bool NetLogSource::IsValid() const {
  return id != kInvalidId;
}

void NetLogSource::AddToEventParameters(base::Value* event_params) const {
  DCHECK(event_params->is_dict());
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("type", static_cast<int>(type));
  dict.SetIntKey("id", static_cast<int>(id));
  event_params->SetKey("source_dependency", std::move(dict));
}

base::Value NetLogSource::ToEventParameters() const {
  return SourceEventParametersCallback(*this);
}

// static
bool NetLogSource::FromEventParameters(const base::Value* event_params,
                                       NetLogSource* source) {
  const base::DictionaryValue* dict = nullptr;
  const base::DictionaryValue* source_dict = nullptr;
  int source_id = -1;
  int source_type = static_cast<int>(NetLogSourceType::COUNT);
  if (!event_params || !event_params->GetAsDictionary(&dict) ||
      !dict->GetDictionary("source_dependency", &source_dict) ||
      !source_dict->GetInteger("id", &source_id) ||
      !source_dict->GetInteger("type", &source_type)) {
    *source = NetLogSource();
    return false;
  }

  DCHECK_GE(source_id, 0);
  DCHECK_LT(source_type, static_cast<int>(NetLogSourceType::COUNT));
  *source = NetLogSource(static_cast<NetLogSourceType>(source_type), source_id);
  return true;
}

}  // namespace net
