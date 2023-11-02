// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import org.chromium.blink.mojom.RemoteObject;
import org.chromium.blink.mojom.RemoteObjectHost;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;

import java.lang.ref.WeakReference;

/**
 * Exposes limited access to a set of Java objects over a Mojo interface.
 *
 * This object is split in two due to the need to ensure that the Mojo watcher does not keep the
 * WebView alive through strong references. It is expected that the WebView be destroyed when it is
 * collected by the runtime.
 *
 * To achieve this, all fields which might transitively point to the WebView are stored in a
 * separate object, RemoteObjectRegistry, held weakly. It is held alive via a retaining set which is
 * owned by the WebView. If the registry is collected, it means that the WebView is gone, and any
 * further access to the Mojo interface should fail.
 *
 * {@link RemoteObjectImpl} similarly holds its target weakly; it is held alive via the map held in
 * Internals.
 */
class RemoteObjectHostImpl implements RemoteObjectHost {
    /**
     * Auditor passed on to {@link RemoteObjectImpl}.
     * Should not hold any strong references that may lead to the contents.
     */
    private final RemoteObjectImpl.Auditor mAuditor;

    /**
     * The registry which owns the underlying target objects, if still alive.
     * See the class comment.
     */
    private final WeakReference<RemoteObjectRegistry> mRegistry;

    private boolean mAllowInspection;

    RemoteObjectHostImpl(RemoteObjectImpl.Auditor auditor, RemoteObjectRegistry registry,
            boolean allowInspection) {
        mAuditor = auditor;
        mRegistry = new WeakReference<>(registry);
        mAllowInspection = allowInspection;
    }

    public void setAllowInspection(boolean allow) {
        mAllowInspection = allow;
    }

    @Override
    public void getObject(int objectId, InterfaceRequest<RemoteObject> request) {
        try (InterfaceRequest<RemoteObject> autoClose = request) {
            RemoteObjectRegistry registry = mRegistry.get();
            if (registry == null) {
                return;
            }
            Object target = registry.getObjectById(objectId);
            if (target == null) {
                return;
            }
            RemoteObjectImpl impl = new RemoteObjectImpl(target,
                    registry.getSafeAnnotationClass(target), mAuditor, registry, mAllowInspection);
            RemoteObject.MANAGER.bind(impl, request);
        }
    }

    @Override
    public void acquireObject(int objectId) {
        RemoteObjectRegistry registry = mRegistry.get();
        if (registry == null) {
            return;
        }
        registry.refObjectById(objectId);
    }

    @Override
    public void releaseObject(int objectId) {
        RemoteObjectRegistry registry = mRegistry.get();
        if (registry == null) {
            return;
        }
        registry.unrefObjectById(objectId);
    }

    @Override
    public void close() {
        RemoteObjectRegistry registry = mRegistry.get();
        if (registry == null) {
            return;
        }
        registry.close();
        mRegistry.clear();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }
}
