// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/dialog_overlay_impl.h"

#include "base/task/post_task.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/DialogOverlayImpl_jni.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_delegate.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

static jlong JNI_DialogOverlayImpl_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jlong high,
                                        jlong low,
                                        jboolean power_efficient) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfhi =
      content::RenderFrameHostImpl::FromOverlayRoutingToken(
          base::UnguessableToken::Deserialize(high, low));

  if (!rfhi)
    return 0;

  // TODO(http://crbug.com/673886): Support overlay surfaces in VR using GVR
  // reprojection video surface.
  RenderWidgetHostViewBase* rwhvb =
      static_cast<RenderWidgetHostViewBase*>(rfhi->GetView());
  if (!rwhvb || rwhvb->IsInVR())
    return 0;

  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      content::WebContents::FromRenderFrameHost(rfhi));

  // If the overlay would not be immediately used, fail the request.
  if (!rfhi->IsCurrent() || !web_contents_impl || web_contents_impl->IsHidden())
    return 0;

  // Dialog-based overlays are not supported for persistent video.
  if (web_contents_impl->HasPersistentVideo())
    return 0;

  // If we require a power-efficient overlay, then approximate that with "is
  // fullscreen".  The reason is that we want to be somewhat sure that we don't
  // have more layers than HWC can support, else SurfaceFlinger will fall back
  // to GLES composition.  In fullscreen mode, the android status bar is hidden,
  // as is the nav bar (if present).  The chrome activity surface also gets
  // hidden when possible.
  if (power_efficient && !web_contents_impl->IsFullscreen())
    return 0;

  return reinterpret_cast<jlong>(
      new DialogOverlayImpl(obj, rfhi, web_contents_impl, power_efficient));
}

DialogOverlayImpl::DialogOverlayImpl(const JavaParamRef<jobject>& obj,
                                     RenderFrameHostImpl* rfhi,
                                     WebContents* web_contents,
                                     bool power_efficient)
    : WebContentsObserver(web_contents),
      rfhi_(rfhi),
      power_efficient_(power_efficient),
      observed_window_android_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(rfhi_);

  JNIEnv* env = AttachCurrentThread();
  obj_ = JavaObjectWeakGlobalRef(env, obj);

  web_contents->GetNativeView()->AddObserver(this);

  // Note that we're not allowed to call back into |obj| before it calls
  // CompleteInit.  However, the observer won't actually call us back until the
  // token changes.  As long as the java side calls us from the ui thread before
  // returning, we won't send a callback before then.
}

void DialogOverlayImpl::CompleteInit(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsDelegate* delegate = web_contents()->GetDelegate();

  if (!delegate) {
    Stop();
    return;
  }

  // Note: It's ok to call SetOverlayMode() directly here, because there can be
  // at most one overlay alive at the time. This logic needs to be updated if
  // ever AndroidOverlayProviderImpl.MAX_OVERLAYS > 1.
  delegate->SetOverlayMode(true);

  // Send the initial token, if there is one.  The observer will notify us about
  // changes only.
  if (auto* window = web_contents()->GetNativeView()->GetWindowAndroid()) {
    RegisterWindowObserverIfNeeded(window);
    ScopedJavaLocalRef<jobject> token = window->GetWindowToken();
    if (!token.is_null()) {
      Java_DialogOverlayImpl_onWindowToken(env, obj, token);
    }
    // else we will send one if we get a callback from ViewAndroid.
  }
}

DialogOverlayImpl::~DialogOverlayImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DialogOverlayImpl::Stop() {
  UnregisterCallbacksIfNeeded();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onDismissed(env, obj);

  obj_.reset();
}

void DialogOverlayImpl::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UnregisterCallbacksIfNeeded();
  // We delete soon since this might be part of an onDismissed callback.
  base::DeleteSoon(FROM_HERE, {BrowserThread::UI}, this);
}

void DialogOverlayImpl::GetCompositorOffset(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& rect) {
  gfx::Point point =
      web_contents()->GetNativeView()->GetLocationOfContainerViewInWindow();

  Java_DialogOverlayImpl_receiveCompositorOffset(env, rect, point.x(),
                                                 point.y());
}

void DialogOverlayImpl::UnregisterCallbacksIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!rfhi_)
    return;

  // We clear overlay mode here rather than in Destroy(), because we may have
  // been called via a WebContentsDestroyed() event, and this might be the last
  // opportunity we have to access web_contents().
  WebContentsDelegate* delegate = web_contents()->GetDelegate();
  if (delegate)
    delegate->SetOverlayMode(false);
  if (observed_window_android_) {
    auto* window_android = web_contents()->GetNativeView()->GetWindowAndroid();
    if (window_android)
      window_android->RemoveObserver(this);
    observed_window_android_ = false;
  }
  web_contents()->GetNativeView()->RemoveObserver(this);
  rfhi_ = nullptr;
}

void DialogOverlayImpl::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host == rfhi_)
    Stop();
}

void DialogOverlayImpl::RenderFrameHostChanged(RenderFrameHost* old_host,
                                               RenderFrameHost* new_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (old_host == rfhi_)
    Stop();
}

void DialogOverlayImpl::FrameDeleted(RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host == rfhi_)
    Stop();
}

void DialogOverlayImpl::OnVisibilityChanged(content::Visibility visibility) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (visibility == content::Visibility::HIDDEN)
    Stop();
}

void DialogOverlayImpl::OnRootWindowVisibilityChanged(bool visible) {
  if (!visible)
    Stop();
}

void DialogOverlayImpl::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Stop();
}

void DialogOverlayImpl::DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                                      bool will_cause_resize) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the caller doesn't care about power-efficient overlays, then don't send
  // any callbacks about state change.
  if (!power_efficient_)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onPowerEfficientState(env, obj, entered_fullscreen);
}

void DialogOverlayImpl::OnAttachedToWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> token;

  if (auto* window = web_contents()->GetNativeView()->GetWindowAndroid()) {
    RegisterWindowObserverIfNeeded(window);
    token = window->GetWindowToken();
  }
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onWindowToken(env, obj, token);
}

void DialogOverlayImpl::OnDetachedFromWindow() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onWindowToken(env, obj, nullptr);
  Stop();
}

void DialogOverlayImpl::RegisterWindowObserverIfNeeded(
    ui::WindowAndroid* window) {
  if (!observed_window_android_) {
    observed_window_android_ = true;
    window->AddObserver(this);
  }
}

static jint JNI_DialogOverlayImpl_RegisterSurface(
    JNIEnv* env,
    const JavaParamRef<jobject>& surface) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(
      gpu::GpuSurfaceTracker::SurfaceRecord(
          gfx::kNullAcceleratedWidget, surface.obj(),
          false /* can_be_used_with_surface_control */));
}

static void JNI_DialogOverlayImpl_UnregisterSurface(
    JNIEnv* env,
    jint surface_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_id);
}

static ScopedJavaLocalRef<jobject>
JNI_DialogOverlayImpl_LookupSurfaceForTesting(
    JNIEnv* env,
    jint surfaceId) {
  bool can_be_used_with_surface_control = false;
  gl::ScopedJavaSurface surface =
      gpu::GpuSurfaceTracker::Get()->AcquireJavaSurface(
          surfaceId, &can_be_used_with_surface_control);
  return ScopedJavaLocalRef<jobject>(surface.j_surface());
}

}  // namespace content
