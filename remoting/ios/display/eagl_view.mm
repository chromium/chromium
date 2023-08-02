// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/display/eagl_view.h"

#include <memory>

#import <OpenGLES/ES2/gl.h>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#import "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace {

// The core that runs on the display thread.
class EAGLViewCore {
 public:
  EAGLViewCore(CAEAGLLayer* layer);
  ~EAGLViewCore();

  void Start(EAGLContext* context);
  void Stop();

  void ReshapeFramebuffer(CGFloat width, CGFloat height);

 private:
  bool IsStarted() const;

  EAGLContext* context_;
  CAEAGLLayer* eagl_layer_;
  GLuint view_frame_buffer_;
  GLuint view_render_buffer_;

  THREAD_CHECKER(thread_checker_);
};

EAGLViewCore::EAGLViewCore(CAEAGLLayer* layer) {
  DETACH_FROM_THREAD(thread_checker_);
  eagl_layer_ = layer;
}

EAGLViewCore::~EAGLViewCore() {
  Stop();
}

void EAGLViewCore::Start(EAGLContext* context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (IsStarted()) {
    Stop();
  }

  context_ = context;

  glGenFramebuffers(1, &view_frame_buffer_);
  glGenRenderbuffers(1, &view_render_buffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, view_frame_buffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, view_render_buffer_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, view_render_buffer_);
}

void EAGLViewCore::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsStarted()) {
    return;
  }

  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteRenderbuffers(1, &view_render_buffer_);
  glDeleteFramebuffers(1, &view_frame_buffer_);
  context_ = nil;
}

void EAGLViewCore::ReshapeFramebuffer(CGFloat width, CGFloat height) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Allocate GL color buffer backing, matching the current layer size
  [context_ renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl_layer_];

  glViewport(0, 0, width, height);
}

bool EAGLViewCore::IsStarted() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_ != nil;
}

}  // namespace

#pragma mark - EAGLView

@interface EAGLView () {
  std::unique_ptr<EAGLViewCore> _core;
}
@end

@implementation EAGLView

@synthesize displayTaskRunner = _displayTaskRunner;

+ (Class)layerClass {
  return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentScaleFactor = [UIScreen mainScreen].scale;
    CAEAGLLayer* eaglLayer = (CAEAGLLayer*)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
      kEAGLDrawablePropertyRetainedBacking : @NO,
      kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8,
    };
    _core.reset(new EAGLViewCore(eaglLayer));
  }
  return self;
}

- (void)dealloc {
  DCHECK(_displayTaskRunner);
  if (!_displayTaskRunner->BelongsToCurrentThread()) {
    _displayTaskRunner->DeleteSoon(FROM_HERE, _core.release());
  }
  // If the current thread is the display thread, _core will be freed by
  // unique_ptr's destructor.
}

- (void)startWithContext:(EAGLContext*)context {
  [self runOnDisplayThread:(base::BindOnce(&EAGLViewCore::Start,
                                           base::Unretained(_core.get()),
                                           context))];
  [self reshapeFrameBuffer];
}

- (void)stop {
  [self runOnDisplayThread:(base::BindOnce(&EAGLViewCore::Stop,
                                           base::Unretained(_core.get())))];
}

#pragma mark - View

- (void)layoutSubviews {
  [self reshapeFrameBuffer];
}

#pragma mark - Properties

- (void)setDisplayTaskRunner:
    (scoped_refptr<base::SingleThreadTaskRunner>)displayTaskRunner {
  DCHECK(!_displayTaskRunner) << "displayTaskRunner is already set.";
  _displayTaskRunner = displayTaskRunner;
}

#pragma mark - Private

// Runs a closure directly if current thread is the display thread. Otherwise
// post a task to do so.
- (void)runOnDisplayThread:(base::OnceClosure)closure {
  DCHECK(_displayTaskRunner) << "displayTaskRunner has not been set.";
  if (_displayTaskRunner->BelongsToCurrentThread()) {
    std::move(closure).Run();
    return;
  }
  _displayTaskRunner->PostTask(FROM_HERE, std::move(closure));
}

- (void)reshapeFrameBuffer {
  CGFloat scaleFactor = self.contentScaleFactor;
  [self runOnDisplayThread:(base::BindOnce(
                               &EAGLViewCore::ReshapeFramebuffer,
                               base::Unretained(_core.get()),
                               scaleFactor * CGRectGetWidth(self.bounds),
                               scaleFactor * CGRectGetHeight(self.bounds)))];
}

@end
