// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DISPLAY_EAGL_VIEW_H_
#define REMOTING_IOS_DISPLAY_EAGL_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

// This is an OpenGL view implementation that allows and guarantees the content
// to be rendered and modified on a non-UI thread. Methods can be called from
// any thread.
@interface EAGLView : UIView

- (instancetype)initWithFrame:(CGRect)frame;

// |context| must be the current EAGLContext of |displayTaskRunner|'s thread.
- (void)startWithContext:(EAGLContext*)context;

- (void)stop;

// The thread to render the content. Must be set once immediately after the view
// is initialized.
@property(nonatomic) scoped_refptr<base::SingleThreadTaskRunner>
    displayTaskRunner;
@end

#endif  // REMOTING_IOS_DISPLAY_EAGL_VIEW_H_
