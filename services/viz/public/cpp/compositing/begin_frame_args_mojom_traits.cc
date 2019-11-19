// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
viz::mojom::BeginFrameArgsType
EnumTraits<viz::mojom::BeginFrameArgsType,
           viz::BeginFrameArgs::BeginFrameArgsType>::
    ToMojom(viz::BeginFrameArgs::BeginFrameArgsType type) {
  switch (type) {
    case viz::BeginFrameArgs::BeginFrameArgsType::INVALID:
      return viz::mojom::BeginFrameArgsType::INVALID;
    case viz::BeginFrameArgs::BeginFrameArgsType::NORMAL:
      return viz::mojom::BeginFrameArgsType::NORMAL;
    case viz::BeginFrameArgs::BeginFrameArgsType::MISSED:
      return viz::mojom::BeginFrameArgsType::MISSED;
  }
  NOTREACHED();
  return viz::mojom::BeginFrameArgsType::INVALID;
}

// static
bool EnumTraits<viz::mojom::BeginFrameArgsType,
                viz::BeginFrameArgs::BeginFrameArgsType>::
    FromMojom(viz::mojom::BeginFrameArgsType input,
              viz::BeginFrameArgs::BeginFrameArgsType* out) {
  switch (input) {
    case viz::mojom::BeginFrameArgsType::INVALID:
      *out = viz::BeginFrameArgs::BeginFrameArgsType::INVALID;
      return true;
    case viz::mojom::BeginFrameArgsType::NORMAL:
      *out = viz::BeginFrameArgs::BeginFrameArgsType::NORMAL;
      return true;
    case viz::mojom::BeginFrameArgsType::MISSED:
      *out = viz::BeginFrameArgs::BeginFrameArgsType::MISSED;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::BeginFrameArgsDataView, viz::BeginFrameArgs>::
    Read(viz::mojom::BeginFrameArgsDataView data, viz::BeginFrameArgs* out) {
  if (!data.ReadFrameTime(&out->frame_time) ||
      !data.ReadDeadline(&out->deadline) ||
      !data.ReadInterval(&out->interval) || !data.ReadType(&out->type)) {
    return false;
  }
  out->source_id = data.source_id();
  out->sequence_number = data.sequence_number();
  out->trace_id = data.trace_id();
  out->on_critical_path = data.on_critical_path();
  out->animate_only = data.animate_only();
  return true;
}

// static
bool StructTraits<viz::mojom::BeginFrameAckDataView, viz::BeginFrameAck>::Read(
    viz::mojom::BeginFrameAckDataView data,
    viz::BeginFrameAck* out) {
  if (data.sequence_number() < viz::BeginFrameArgs::kStartingFrameNumber)
    return false;
  out->source_id = data.source_id();
  out->sequence_number = data.sequence_number();
  out->trace_id = data.trace_id();
  out->has_damage = data.has_damage();
  return true;
}

}  // namespace mojo
