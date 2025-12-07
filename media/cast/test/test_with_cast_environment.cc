// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/test_with_cast_environment.h"

namespace media::cast {

WithCastEnvironment::WithCastEnvironment()
    : cast_environment_(base::MakeRefCounted<CastEnvironment>(
          *task_environment_.GetMockTickClock(),
          task_environment_.GetMainThreadTaskRunner(),
          task_environment_.GetMainThreadTaskRunner(),
          task_environment_.GetMainThreadTaskRunner(),

          // NOTE: Unretained is safe because we wait for this task before
          // deleting `this`.
          base::BindOnce(&WithCastEnvironment::OnCastEnvironmentDeletion,
                         base::Unretained(this)))) {}

void WithCastEnvironment::OnCastEnvironmentDeletion() {
  CHECK(deletion_cb_);
  std::move(deletion_cb_).Run();
}

WithCastEnvironment::~WithCastEnvironment() {
  deletion_cb_ = task_environment_.QuitClosure();
  cast_environment_.reset();
  task_environment_.RunUntilQuit();
}

TestWithCastEnvironment::~TestWithCastEnvironment() = default;

}  // namespace media::cast
