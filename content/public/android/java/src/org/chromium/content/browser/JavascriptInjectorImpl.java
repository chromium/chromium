// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.origin_matcher.OriginMatcher;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Implementation class of the interface {@link JavascriptInjector}. */
@JNINamespace("content")
@NullMarked
public class JavascriptInjectorImpl implements JavascriptInjector, UserData {
    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<JavascriptInjectorImpl> INSTANCE =
                JavascriptInjectorImpl::new;
    }

    // The set is passed to native and stored in a weak reference, so ensure this
    // strong reference is not optimized away by R8.
    @DoNotInline private final Set<Object> mRetainedObjects = new HashSet<>();
    private final Map<String, InjectedInterface> mInjectedObjects = new HashMap<>();
    private long mNativePtr;

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link JavascriptInjector} object used for the give WebContents. Creates one if not
     *     present.
     */
    public static @Nullable JavascriptInjector fromWebContents(WebContents webContents) {
        JavascriptInjectorImpl javascriptInjector =
                webContents.getOrSetUserData(
                        JavascriptInjectorImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        return javascriptInjector;
    }

    public JavascriptInjectorImpl(WebContents webContents) {
        mNativePtr = JavascriptInjectorImplJni.get().init(this, webContents, mRetainedObjects);
    }

    @CalledByNative
    private void onDestroy() {
        mNativePtr = 0;
    }

    @Override
    public Map<String, InjectedInterface> getInterfaces() {
        return mInjectedObjects;
    }

    @Override
    public void setAllowInspection(boolean allow) {
        if (mNativePtr != 0) {
            JavascriptInjectorImplJni.get().setAllowInspection(mNativePtr, allow);
        }
    }

    @Override
    public void addPossiblyUnsafeInterfaceToOrigins(
            @Nullable Object object,
            String name,
            @Nullable Class<? extends Annotation> requiredAnnotation,
            OriginMatcher matcher) {
        if (object == null || mNativePtr == 0) {
            return;
        }

        mInjectedObjects.put(
                name, new InjectedInterface(object, requiredAnnotation, matcher.serialize()));
        JavascriptInjectorImplJni.get()
                .addInterface(mNativePtr, object, name, requiredAnnotation, matcher);
    }

    @Override
    public void addPossiblyUnsafeInterface(
            @Nullable Object object,
            String name,
            @Nullable Class<? extends Annotation> requiredAnnotation) {
        OriginMatcher matcher = new OriginMatcher();
        try {
            matcher.setRuleList(List.of("*"));
            addPossiblyUnsafeInterfaceToOrigins(object, name, requiredAnnotation, matcher);
            // We always need to clean the matcher when we
            // are done with it.
        } finally {
            matcher.destroy();
        }
    }

    @Override
    public void removeInterface(String name) {
        mInjectedObjects.remove(name);
        if (mNativePtr != 0) {
            JavascriptInjectorImplJni.get().removeInterface(mNativePtr, name);
        }
    }

    @NativeMethods
    interface Natives {
        long init(JavascriptInjectorImpl self, WebContents webContents, Object retainedObjects);

        void setAllowInspection(long nativeJavascriptInjector, boolean allow);

        void addInterface(
                long nativeJavascriptInjector,
                Object object,
                String name,
                @Nullable Class requiredAnnotation,
                @JniType("origin_matcher::OriginMatcher") OriginMatcher matcher);

        void removeInterface(long nativeJavascriptInjector, String name);
    }
}
