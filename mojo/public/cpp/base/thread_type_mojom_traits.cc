// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/thread_type_mojom_traits.h"

#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace mojo {

// static
mojo_base::mojom::ThreadType
EnumTraits<mojo_base::mojom::ThreadType, base::ThreadType>::ToMojom(
    base::ThreadType thread_type) {
  switch (thread_type) {
    case base::ThreadType::kBackground:
      return mojo_base::mojom::ThreadType::kBackground;
    case base::ThreadType::kUtility:
      return mojo_base::mojom::ThreadType::kUtility;
    case base::ThreadType::kDefault:
      return mojo_base::mojom::ThreadType::kDefault;
    case base::ThreadType::kPresentation:
      return mojo_base::mojom::ThreadType::kPresentation;
    case base::ThreadType::kAudioProcessing:
      return mojo_base::mojom::ThreadType::kAudioProcessing;
    case base::ThreadType::kRealtimeAudio:
      return mojo_base::mojom::ThreadType::kRealtimeAudio;
  }
  NOTREACHED();
}

// static
base::ThreadType
EnumTraits<mojo_base::mojom::ThreadType, base::ThreadType>::FromMojom(
    mojo_base::mojom::ThreadType input) {
  switch (input) {
    case mojo_base::mojom::ThreadType::kBackground:
      return base::ThreadType::kBackground;
    case mojo_base::mojom::ThreadType::kUtility:
      return base::ThreadType::kUtility;
    case mojo_base::mojom::ThreadType::kDefault:
      return base::ThreadType::kDefault;
    case mojo_base::mojom::ThreadType::kPresentation:
      return base::ThreadType::kPresentation;
    case mojo_base::mojom::ThreadType::kAudioProcessing:
      return base::ThreadType::kAudioProcessing;
    case mojo_base::mojom::ThreadType::kRealtimeAudio:
      return base::ThreadType::kRealtimeAudio;
  }
  NOTREACHED();
}

}  // namespace mojo
