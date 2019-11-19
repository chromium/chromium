// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/fake_canvas.h"

namespace remoting {

FakeCanvas::FakeCanvas() {}

FakeCanvas::~FakeCanvas() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeCanvas::Clear() {}

void FakeCanvas::SetTransformationMatrix(const std::array<float, 9>& matrix) {}

void FakeCanvas::SetViewSize(int width, int height) {}

void FakeCanvas::DrawTexture(int texture_id,
                             int texture_handle,
                             int vertex_buffer,
                             float alpha_multiplier) {}

int FakeCanvas::GetVersion() const {
  return 0;
}

int FakeCanvas::GetMaxTextureSize() const {
  return 0;
}

base::WeakPtr<Canvas> FakeCanvas::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
