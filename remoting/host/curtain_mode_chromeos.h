// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_
#define REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_

#include "remoting/host/curtain_mode.h"

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"

namespace ash::curtain {
class SecurityCurtainController;
}  // namespace ash::curtain

namespace remoting {

// Helper class that handles everything related to curtained sessions on
// ChromeOS, which includes:
//    - Creating a virtual display
//    - Installing the curtain screen
//    - Suppressing local input
class CurtainModeChromeOs : public CurtainMode {
 public:
  explicit CurtainModeChromeOs(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  CurtainModeChromeOs(const CurtainModeChromeOs&) = delete;
  CurtainModeChromeOs& operator=(const CurtainModeChromeOs&) = delete;
  ~CurtainModeChromeOs() override;

  static ash::curtain::SecurityCurtainController::InitParams CreateInitParams();

  // CurtainMode implementation:
  bool Activate() override;

 private:
  class Core {
   public:
    ~Core();

    void Activate();

   private:
    ash::curtain::SecurityCurtainController& security_curtain_controller();
  };

  // Implementation of this curtain mode that ensures everything we do
  // is executed on the ui thread.
  base::SequenceBound<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_
