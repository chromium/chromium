// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
#define REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import <memory>

#import "remoting/client/display/sys_opengl.h"

namespace remoting {

class ChromotingClientRuntime;
class RendererProxy;

namespace protocol {

class VideoRenderer;
class CursorShapeStub;

}  // namespace protocol
}  // namespace remoting

@class EAGLView;

// This protocol is for receiving notifications from the renderer when its state
// changes. Implementations can use this to reposition viewport, process
// animations, etc.
@protocol GlDisplayHandlerDelegate<NSObject>

// Notifies the delegate that the size of the desktop image has changed.
- (void)canvasSizeChanged:(CGSize)size;

- (void)rendererTicked;

@end

@interface GlDisplayHandler : NSObject {
}

// Called once the renderer can start drawing on the view. Do nothing if the
// surface is already created.
- (void)createRendererContext:(EAGLView*)view;

// Called when the renderer should stop drawing on the view. Do nothing if the
// surface is not created.
- (void)destroyRendererContext;

// Called every time the view dimension is initialized or changed.
- (void)setSurfaceSize:(const CGRect&)frame;

// Must be called immediately after the object is constructed.
- (std::unique_ptr<remoting::protocol::VideoRenderer>)createVideoRenderer;
- (std::unique_ptr<remoting::protocol::CursorShapeStub>)createCursorShapeStub;

@property(readonly) remoting::RendererProxy* rendererProxy;

// This is write-only but @property doesn't support write-only modifier.
@property id<GlDisplayHandlerDelegate> delegate;
- (id<GlDisplayHandlerDelegate>)delegate UNAVAILABLE_ATTRIBUTE;

@end

#endif  // REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
