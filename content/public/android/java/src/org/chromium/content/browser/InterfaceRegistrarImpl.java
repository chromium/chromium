// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink.mojom.AndroidFontLookup;
import org.chromium.content.browser.androidoverlay.AndroidOverlayProviderImpl;
import org.chromium.content.browser.font.AndroidFontLookupImpl;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.media.mojom.AndroidOverlayProvider;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.services.service_manager.InterfaceRegistry;

@JNINamespace("content")
class InterfaceRegistrarImpl {

    private static boolean sHasRegisteredRegistrars;

    @CalledByNative
    static void createInterfaceRegistryForContext(int nativeHandle) {
        ensureContentRegistrarsAreRegistered();

        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyContextRegistrars(registry);
    }

    @CalledByNative
    static void createInterfaceRegistryForWebContents(int nativeHandle, WebContents webContents) {
        ensureContentRegistrarsAreRegistered();

        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyWebContentsRegistrars(registry, webContents);
    }

    @CalledByNative
    static void createInterfaceRegistryForRenderFrameHost(
            int nativeHandle, RenderFrameHost renderFrameHost) {
        ensureContentRegistrarsAreRegistered();

        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyRenderFrameHostRegistrars(registry, renderFrameHost);
    }

    private static void ensureContentRegistrarsAreRegistered() {
        if (sHasRegisteredRegistrars) return;
        sHasRegisteredRegistrars = true;
        InterfaceRegistrar.Registry.addContextRegistrar(new ContentContextInterfaceRegistrar());
    }

    private static class ContentContextInterfaceRegistrar implements InterfaceRegistrar<Context> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final Context applicationContext) {
            registry.addInterface(AndroidOverlayProvider.MANAGER,
                    new AndroidOverlayProviderImpl.Factory(applicationContext));
            // TODO(avayvod): Register the PresentationService implementation here.
            registry.addInterface(AndroidFontLookup.MANAGER,
                    new AndroidFontLookupImpl.Factory(applicationContext));
        }
    }
}
