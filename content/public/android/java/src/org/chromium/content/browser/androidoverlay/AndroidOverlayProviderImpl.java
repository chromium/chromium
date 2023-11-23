// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;
import org.chromium.media.mojom.AndroidOverlay;
import org.chromium.media.mojom.AndroidOverlayClient;
import org.chromium.media.mojom.AndroidOverlayConfig;
import org.chromium.media.mojom.AndroidOverlayProvider;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Default impl of AndroidOverlayProvider.  Creates AndroidOverlayImpls.  We're a singleton, in the
 * sense that all provider clients talk to the same instance in the browser.
 */
@JNINamespace("content")
public class AndroidOverlayProviderImpl implements AndroidOverlayProvider {
    private static final String TAG = "AndroidOverlayProvider";

    // Maximum number of concurrent overlays that we allow.
    // Note: DialogOverlayImpl::CompleteInit() calls WebContentsDelegate::SetOverlayMode() directly,
    // because there can only be one overlay alive at a time. If we were to support multiple
    // concurrent overlays, we need to revisit this logic.
    private static final int MAX_OVERLAYS = 1;

    // Number of AndroidOverlays that have been created but not released.
    private int mNumOverlays;

    // Runnable that notifies us that a client has been released.
    private Runnable mNotifyReleasedRunnable =
            new Runnable() {
                @Override
                public void run() {
                    notifyReleased();
                }
            };

    /**
     * Create an overlay matching |config| for |client|, and bind it to |request|.  Remember that
     * potentially many providers are created.
     */
    @Override
    public void createOverlay(
            InterfaceRequest<AndroidOverlay> request,
            AndroidOverlayClient client,
            AndroidOverlayConfig config) {
        ThreadUtils.assertOnUiThread();

        // If this is no longer true, we need to update DialogOverlayImpl::CompleteInit().
        assert MAX_OVERLAYS == 1;

        // Limit the number of concurrent surfaces.
        if (mNumOverlays >= MAX_OVERLAYS) {
            client.onDestroyed();
            client.close();
            return;
        }

        mNumOverlays++;

        DialogOverlayImpl impl =
                new DialogOverlayImpl(
                        client, config, mNotifyReleasedRunnable, /* asPanel= */ false);
        DialogOverlayImpl.MANAGER.bind(impl, request);
    }

    /**
     * Called by AndroidOverlays when they no longer need the thread via |mNotifyReleasedRunnable|.
     */
    private void notifyReleased() {
        ThreadUtils.assertOnUiThread();
        assert mNumOverlays > 0;
        mNumOverlays--;

        // We don't stop the looper thread here, else android can get mad when it tries to send
        // a message from the dialog on this thread.  AndroidOverlay might have to notify us
        // separately to tell us when it's done with the thread, if we don't want to wait until
        // then to start creating a new SV.
        // Instead, we just avoid shutting down the thread at all for now.
    }

    // Remember that we can't tell which client disconnected.
    @Override
    public void close() {}

    // Remember that we can't tell which client disconnected.
    @Override
    public void onConnectionError(MojoException e) {}

    // Are overlays supported by the embedder?
    @CalledByNative
    private static boolean areOverlaysSupported() {
        return true;
    }

    /** Mojo factory. */
    public static class Factory implements InterfaceFactory<AndroidOverlayProvider> {
        private static AndroidOverlayProviderImpl sImpl;

        public Factory() {}

        @Override
        public AndroidOverlayProvider createImpl() {
            if (sImpl == null) sImpl = new AndroidOverlayProviderImpl();
            return sImpl;
        }
    }
}
