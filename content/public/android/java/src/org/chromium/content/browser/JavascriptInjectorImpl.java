// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Pair;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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

    private final Set<Object> mRetainedObjects = new HashSet<>();
    private final Map<String, Pair<Object, Class>> mInjectedObjects = new HashMap<>();
    private long mNativePtr;

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link JavascriptInjector} object used for the give WebContents.
     *         Creates one if not present.
     */
    public static JavascriptInjector fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(JavascriptInjectorImpl.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    public JavascriptInjectorImpl(WebContents webContents) {
        mNativePtr = JavascriptInjectorImplJni.get().init(
                JavascriptInjectorImpl.this, webContents, mRetainedObjects);
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
        if (mNativePtr != 0)
            JavascriptInjectorImplJni.get().setAllowInspection(
                    mNativePtr, JavascriptInjectorImpl.this, allow);
    }

    @Override
    public void addPossiblyUnsafeInterface(
            Object object, String name, Class<? extends Annotation> requiredAnnotation) {
        if (mNativePtr != 0 && object != null) {
            mInjectedObjects.put(name, new Pair<Object, Class>(object, requiredAnnotation));
            JavascriptInjectorImplJni.get().addInterface(
                    mNativePtr, JavascriptInjectorImpl.this, object, name, requiredAnnotation);
        }
    }

    @Override
    public void removeInterface(String name) {
        mInjectedObjects.remove(name);
        if (mNativePtr != 0)
            JavascriptInjectorImplJni.get().removeInterface(
                    mNativePtr, JavascriptInjectorImpl.this, name);
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
