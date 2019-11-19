// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIELD_TRIAL_RECORDER_H_
#define CONTENT_BROWSER_FIELD_TRIAL_RECORDER_H_

#include "base/threading/thread_checker.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class FieldTrialRecorder : public mojom::FieldTrialRecorder {
 public:
  FieldTrialRecorder();
  ~FieldTrialRecorder() override;

  static void Create(mojo::PendingReceiver<mojom::FieldTrialRecorder> receiver);

 private:
  // content::mojom::FieldTrialRecorder:
  void FieldTrialActivated(const std::string& trial_name) override;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIELD_TRIAL_RECORDER_H_
