// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CREATE_DESKTOP_INTERACTION_STRATEGY_FACTORY_H_
#define REMOTING_HOST_CREATE_DESKTOP_INTERACTION_STRATEGY_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/desktop_interaction_strategy.h"

namespace remoting {

std::unique_ptr<DesktopInteractionStrategyFactory>
CreateDesktopInteractionStrategyFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner);
}

#endif  // REMOTING_HOST_CREATE_DESKTOP_INTERACTION_STRATEGY_FACTORY_H_
