// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Pair;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.content.browser.remoteobjects.RemoteObjectInjector;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Implementation class of the interface {@link JavascriptInjector}.
 */
@JNINamespace("content")
public class JavascriptInjectorImpl implements JavascriptInjector, UserData {
    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<JavascriptInjectorImpl> INSTANCE =
                JavascriptInjectorImpl::new;
    }

    // The set is passed to native and stored in a weak reference, so ensure this
    // strong reference is not optimized away by R8.
    @DoNotInline
    private final Set<Object> mRetainedObjects = new HashSet<>();
    private final Map<String, Pair<Object, Class>> mInjectedObjects = new HashMap<>();
    private long mNativePtr;
    private RemoteObjectInjector mInjector;
    private Boolean mUseMojo;

    /**
     * @param webContents {@link WebContents} object.
     * @param useMojo Whether to use {@link RemoteObjectInjector} methods
     * @return {@link JavascriptInjector} object used for the give WebContents.
     *         Creates one if not present.
     */
    public static JavascriptInjector fromWebContents(WebContents webContents, boolean useMojo) {
        JavascriptInjectorImpl javascriptInjector =
                ((WebContentsImpl) webContents)
                        .getOrSetUserData(
                                JavascriptInjectorImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        javascriptInjector.setUseMojo(useMojo);
        return javascriptInjector;
    }

    public JavascriptInjectorImpl(WebContents webContents) {
        mNativePtr = JavascriptInjectorImplJni.get().init(
                JavascriptInjectorImpl.this, webContents, mRetainedObjects);
        mInjector = new RemoteObjectInjector(webContents);
        webContents.addObserver(mInjector);
    }

    public void setUseMojo(boolean useMojo) {
        if (mUseMojo == null) {
            mUseMojo = useMojo;
        } else {
            assert mUseMojo == useMojo;
        }
    }

    @CalledByNative
    private void onDestroy() {
        mNativePtr = 0;
    }

    @Override
    public Map<String, Pair<Object, Class>> getInterfaces() {
        return mInjectedObjects;
    }

    @Override
    public void setAllowInspection(boolean allow) {
        assert mUseMojo != null;
        if (mUseMojo) {
            mInjector.setAllowInspection(allow);
        } else if (mNativePtr != 0) {
            JavascriptInjectorImplJni.get().setAllowInspection(
                    mNativePtr, JavascriptInjectorImpl.this, allow);
        }
    }

    @Override
    public void addPossiblyUnsafeInterface(
            Object object, String name, Class<? extends Annotation> requiredAnnotation) {
        if (object == null) return;

        assert mUseMojo != null;
        if (mUseMojo) {
            mInjector.addInterface(object, name, requiredAnnotation);
        } else if (mNativePtr != 0) {
            mInjectedObjects.put(name, new Pair<Object, Class>(object, requiredAnnotation));
            JavascriptInjectorImplJni.get().addInterface(
                    mNativePtr, JavascriptInjectorImpl.this, object, name, requiredAnnotation);
        }
    }

    @Override
    public void removeInterface(String name) {
        assert mUseMojo != null;
        if (mUseMojo) {
            mInjector.removeInterface(name);
        } else {
            mInjectedObjects.remove(name);
            if (mNativePtr != 0) {
                JavascriptInjectorImplJni.get().removeInterface(
                        mNativePtr, JavascriptInjectorImpl.this, name);
            }
        }
    }

    @NativeMethods
    interface Natives {
        long init(JavascriptInjectorImpl caller, WebContents webContents, Object retainedObjects);
        void setAllowInspection(
                long nativeJavascriptInjector, JavascriptInjectorImpl caller, boolean allow);
        void addInterface(long nativeJavascriptInjector, JavascriptInjectorImpl caller,
                Object object, String name, Class requiredAnnotation);
        void removeInterface(
                long nativeJavascriptInjector, JavascriptInjectorImpl caller, String name);
    }
}
