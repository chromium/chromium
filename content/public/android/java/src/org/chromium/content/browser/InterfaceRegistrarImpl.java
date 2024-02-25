// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

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
    static void createInterfaceRegistry(long nativeHandle) {
        ensureSingletonRegistrarsAreRegistered();

        InterfaceRegistry registry =
                InterfaceRegistry.create(
                        CoreImpl.getInstance()
                                .acquireNativeHandle(nativeHandle)
                                .toMessagePipeHandle());
        InterfaceRegistrar.Registry.applySingletonRegistrars(registry);
    }

    @CalledByNative
    static void createInterfaceRegistryOnIOThread(long nativeHandle) {
        InterfaceRegistry registry =
                InterfaceRegistry.create(
                        CoreImpl.getInstance()
                                .acquireNativeHandle(nativeHandle)
                                .toMessagePipeHandle());
        registerInterfacesOnIOThread(registry);
    }

    @CalledByNative
    static void createInterfaceRegistryForWebContents(long nativeHandle, WebContents webContents) {
        ensureSingletonRegistrarsAreRegistered();

        InterfaceRegistry registry =
                InterfaceRegistry.create(
                        CoreImpl.getInstance()
                                .acquireNativeHandle(nativeHandle)
                                .toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyWebContentsRegistrars(registry, webContents);
    }

    @CalledByNative
    static void createInterfaceRegistryForRenderFrameHost(
            long nativeHandle, RenderFrameHost renderFrameHost) {
        ensureSingletonRegistrarsAreRegistered();

        InterfaceRegistry registry =
                InterfaceRegistry.create(
                        CoreImpl.getInstance()
                                .acquireNativeHandle(nativeHandle)
                                .toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyRenderFrameHostRegistrars(registry, renderFrameHost);
    }

    private static void ensureSingletonRegistrarsAreRegistered() {
        if (sHasRegisteredRegistrars) return;
        sHasRegisteredRegistrars = true;
        InterfaceRegistrar.Registry.addSingletonRegistrar(new SingletonInterfaceRegistrar());
    }

    private static void registerInterfacesOnIOThread(InterfaceRegistry registry) {
        registry.addInterface(AndroidFontLookup.MANAGER, new AndroidFontLookupImpl.Factory());
    }

    private static class SingletonInterfaceRegistrar implements InterfaceRegistrar<Void> {
        @Override
        public void registerInterfaces(InterfaceRegistry registry, Void v) {
            registry.addInterface(
                    AndroidOverlayProvider.MANAGER, new AndroidOverlayProviderImpl.Factory());
            // TODO(avayvod): Register the PresentationService implementation here.
            registry.addInterface(AndroidFontLookup.MANAGER, new AndroidFontLookupImpl.Factory());
        }
    }
}
