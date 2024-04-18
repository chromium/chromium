// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_IN_PROGRESS_STROKE_H_
#define PDF_INK_INK_IN_PROGRESS_STROKE_H_

#include <memory>

namespace chrome_pdf {

class InkBrush;
class InkStroke;
class InkStrokeInputBatch;

class InkInProgressStroke {
 public:
  static std::unique_ptr<InkInProgressStroke> Create();

  InkInProgressStroke(const InkInProgressStroke&) = delete;
  InkInProgressStroke& operator=(const InkInProgressStroke&) = delete;
  virtual ~InkInProgressStroke() = default;

  virtual void Start(const InkBrush& brush) = 0;

  virtual bool EnqueueInputs(const InkStrokeInputBatch* real_inputs,
                             const InkStrokeInputBatch* predicted_inputs) = 0;

  virtual void FinishInputs() = 0;

  virtual bool UpdateShape(float current_elapsed_time_seconds) = 0;

  virtual std::unique_ptr<InkStroke> CopyToStroke() const = 0;

 protected:
  InkInProgressStroke() = default;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_IN_PROGRESS_STROKE_H_
