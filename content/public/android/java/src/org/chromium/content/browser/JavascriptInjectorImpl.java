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
    private final WebContents mWebContents;
    private boolean mNativeDestroyed;

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
        mWebContents = webContents;
        JavascriptInjectorImplJni.get().init(this, webContents, mRetainedObjects);
    }

    @CalledByNative
    private void onDestroy() {
        mNativeDestroyed = true;
    }

    @Override
    public Map<String, InjectedInterface> getInterfaces() {
        return mInjectedObjects;
    }

    @Override
    public void setAllowInspection(boolean allow) {
        if (!mNativeDestroyed && !mWebContents.isDestroyed()) {
            JavascriptInjectorImplJni.get().setAllowInspection(mWebContents, allow);
        }
    }

    @Override
    public void addPossiblyUnsafeInterfaceToOrigins(
            @Nullable Object object,
            String name,
            @Nullable Class<? extends Annotation> requiredAnnotation,
            OriginMatcher matcher) {
        if (object == null || mNativeDestroyed || mWebContents.isDestroyed()) {
            return;
        }

        mInjectedObjects.put(
                name, new InjectedInterface(object, requiredAnnotation, matcher.serialize()));
        JavascriptInjectorImplJni.get()
                .addInterface(mWebContents, object, name, requiredAnnotation, matcher);
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
        if (!mNativeDestroyed && !mWebContents.isDestroyed()) {
            JavascriptInjectorImplJni.get().removeInterface(mWebContents, name);
        }
    }

    @NativeMethods
    interface Natives {
        void init(JavascriptInjectorImpl self, WebContents webContents, Object retainedObjects);

        void setAllowInspection(
                @JniType("content::WebContents*") WebContents webContents, boolean allow);

        void addInterface(
                @JniType("content::WebContents*") WebContents webContents,
                Object object,
                String name,
                @Nullable Class requiredAnnotation,
                @JniType("origin_matcher::OriginMatcher") OriginMatcher matcher);

        void removeInterface(
                @JniType("content::WebContents*") WebContents webContents, String name);
    }
}
