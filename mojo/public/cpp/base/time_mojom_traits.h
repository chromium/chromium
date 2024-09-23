// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_TIME_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_TIME_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "mojo/public/mojom/base/time.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::TimeDataView, base::Time> {
  static int64_t internal_value(const base::Time& time);

  static bool Read(mojo_base::mojom::TimeDataView data, base::Time* time);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::JSTimeDataView, base::Time> {
  static double msec(const base::Time& time);

  static bool Read(mojo_base::mojom::JSTimeDataView data, base::Time* time);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::TimeDeltaDataView, base::TimeDelta> {
  static int64_t microseconds(const base::TimeDelta& delta);

  static bool Read(mojo_base::mojom::TimeDeltaDataView data,
                   base::TimeDelta* delta);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::TimeTicksDataView, base::TimeTicks> {
  static int64_t internal_value(const base::TimeTicks& time);

  static bool Read(mojo_base::mojom::TimeTicksDataView data,
                   base::TimeTicks* time);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_TIME_MOJOM_TRAITS_H_
