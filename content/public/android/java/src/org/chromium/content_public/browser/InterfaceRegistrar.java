// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.services.service_manager.InterfaceRegistry;

import java.util.ArrayList;
import java.util.List;

/**
 * A registrar for mojo interface implementations to provide to an InterfaceRegistry.
 *
 * @param <ParamType> the type of parameter to pass to the InterfaceRegistrar when adding its
 *     interfaces to an InterfaceRegistry
 */
public interface InterfaceRegistrar<ParamType> {
    /** Invoked to register interfaces on |registry|, parametrized by |paramValue|. */
    public void registerInterfaces(InterfaceRegistry registry, ParamType paramValue);

    /** A registry of InterfaceRegistrars. */
    public static class Registry<ParamType> {
        private static Registry<Void> sSingletonRegistry;
        private static Registry<WebContents> sWebContentsRegistry;
        private static Registry<RenderFrameHost> sRenderFrameHostRegistry;

        private List<InterfaceRegistrar<ParamType>> mRegistrars =
                new ArrayList<InterfaceRegistrar<ParamType>>();

        public static void applySingletonRegistrars(InterfaceRegistry interfaceRegistry) {
            if (sSingletonRegistry == null) {
                return;
            }
            sSingletonRegistry.applyRegistrars(interfaceRegistry, null);
        }

        public static void applyWebContentsRegistrars(
                InterfaceRegistry interfaceRegistry, WebContents webContents) {
            if (sWebContentsRegistry == null) {
                return;
            }
            sWebContentsRegistry.applyRegistrars(interfaceRegistry, webContents);
        }

        public static void applyRenderFrameHostRegistrars(
                InterfaceRegistry interfaceRegistry, RenderFrameHost renderFrameHost) {
            if (sRenderFrameHostRegistry == null) {
                return;
            }
            sRenderFrameHostRegistry.applyRegistrars(interfaceRegistry, renderFrameHost);
        }

        public static void addSingletonRegistrar(InterfaceRegistrar<Void> registrar) {
            if (sSingletonRegistry == null) {
                sSingletonRegistry = new Registry<>();
            }
            sSingletonRegistry.addRegistrar(registrar);
        }

        public static void addWebContentsRegistrar(InterfaceRegistrar<WebContents> registrar) {
            if (sWebContentsRegistry == null) {
                sWebContentsRegistry = new Registry<WebContents>();
            }
            sWebContentsRegistry.addRegistrar(registrar);
        }

        public static void addRenderFrameHostRegistrar(
                InterfaceRegistrar<RenderFrameHost> registrar) {
            if (sRenderFrameHostRegistry == null) {
                sRenderFrameHostRegistry = new Registry<RenderFrameHost>();
            }
            sRenderFrameHostRegistry.addRegistrar(registrar);
        }

        private Registry() {}

        private void addRegistrar(InterfaceRegistrar<ParamType> registrar) {
            mRegistrars.add(registrar);
        }

        private void applyRegistrars(InterfaceRegistry interfaceRegistry, ParamType param) {
            for (InterfaceRegistrar<ParamType> registrar : mRegistrars) {
                registrar.registerInterfaces(interfaceRegistry, param);
            }
        }
    }
}
