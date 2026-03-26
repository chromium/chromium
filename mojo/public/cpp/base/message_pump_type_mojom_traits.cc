// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/message_pump_type_mojom_traits.h"

#include "base/notreached.h"
#include "build/build_config.h"

namespace mojo {

// static
mojo_base::mojom::MessagePumpType
EnumTraits<mojo_base::mojom::MessagePumpType, base::MessagePumpType>::ToMojom(
    base::MessagePumpType input) {
  switch (input) {
    case base::MessagePumpType::DEFAULT:
      return mojo_base::mojom::MessagePumpType::kDefault;
    case base::MessagePumpType::UI:
      return mojo_base::mojom::MessagePumpType::kUi;
    case base::MessagePumpType::CUSTOM:
      return mojo_base::mojom::MessagePumpType::kCustom;
    case base::MessagePumpType::IO:
      return mojo_base::mojom::MessagePumpType::kIo;
#if BUILDFLAG(IS_ANDROID)
    case base::MessagePumpType::JAVA:
      return mojo_base::mojom::MessagePumpType::kJava;
#endif
#if BUILDFLAG(IS_APPLE)
    case base::MessagePumpType::NS_RUNLOOP:
      return mojo_base::mojom::MessagePumpType::kNsRunloop;
#endif
  }
  NOTREACHED();
}

// static
base::MessagePumpType
EnumTraits<mojo_base::mojom::MessagePumpType, base::MessagePumpType>::FromMojom(
    mojo_base::mojom::MessagePumpType input) {
  switch (input) {
    case mojo_base::mojom::MessagePumpType::kDefault:
      return base::MessagePumpType::DEFAULT;
    case mojo_base::mojom::MessagePumpType::kUi:
      return base::MessagePumpType::UI;
    case mojo_base::mojom::MessagePumpType::kCustom:
      return base::MessagePumpType::CUSTOM;
    case mojo_base::mojom::MessagePumpType::kIo:
      return base::MessagePumpType::IO;
#if BUILDFLAG(IS_ANDROID)
    case mojo_base::mojom::MessagePumpType::kJava:
      return base::MessagePumpType::JAVA;
#endif
#if BUILDFLAG(IS_APPLE)
    case mojo_base::mojom::MessagePumpType::kNsRunloop:
      return base::MessagePumpType::NS_RUNLOOP;
#endif
  }
  NOTREACHED();
}

}  // namespace mojo
