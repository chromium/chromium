// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.util.Pair;

import org.chromium.content.browser.JavascriptInjectorImpl;

import java.lang.annotation.Annotation;
import java.util.Map;

/**
 * Interface that provides API used to inject user-defined objects that allow custom Javascript
 * interfaces.
 */
public interface JavascriptInjector {
    /**
     * @param webContents {@link WebContents} object.
     * @return {@link JavascriptInjector} object used for the give WebContents. Creates one if not
     *     present.
     */
    static JavascriptInjector fromWebContents(WebContents webContents) {
        return JavascriptInjectorImpl.fromWebContents(webContents);
    }

    /**
     * Returns Javascript interface objects previously injected via {@link
     * #addPossiblyUnsafeInterface(Object, String)}.
     *
     * @return the mapping of names to interface objects and corresponding annotation classes
     */
    Map<String, Pair<Object, Class>> getInterfaces();

    /**
     * Enables or disables inspection of Javascript objects added via
     * {@link #addInterface(Object, String)} by means of Object.keys() method and
     * &quot;for .. in&quot; loop. Being able to inspect Javascript objects is useful
     * when debugging hybrid Android apps, but can't be enabled for legacy applications due
     * to compatibility risks.
     *
     * @param allow Whether to allow Javascript objects inspection.
     */
    void setAllowInspection(boolean allow);

    /**
     * This method injects the supplied Java object into the WebContents.
     * The object is injected into the Javascript context of the main frame,
     * using the supplied name. This allows the Java object to be accessed from
     * Javascript. Note that that injected objects will not appear in
     * Javascript until the page is next (re)loaded. For example:
     * <pre> view.addJavascriptInterface(new Object(), "injectedObject");
     * view.loadData("<!DOCTYPE html><title></title>", "text/html", null);
     * view.loadUrl("javascript:alert(injectedObject.toString())");</pre>
     * <p><strong>IMPORTANT:</strong>
     * <ul>
     * <li> addInterface() can be used to allow Javascript to control
     * the host application. This is a powerful feature, but also presents a
     * security risk. Use of this method in a WebContents containing
     * untrusted content could allow an attacker to manipulate the host
     * application in unintended ways, executing Java code with the permissions
     * of the host application. Use extreme care when using this method in a
     * WebContents which could contain untrusted content. Particular care
     * should be taken to avoid unintentional access to inherited methods, such
     * as {@link Object#getClass()}. To prevent access to inherited methods,
     * pass an annotation for {@code requiredAnnotation}.  This will ensure
     * that only methods with {@code requiredAnnotation} are exposed to the
     * Javascript layer.  {@code requiredAnnotation} will be passed to all
     * subsequently injected Java objects if any methods return an object.  This
     * means the same restrictions (or lack thereof) will apply.  Alternatively,
     * {@link #addInterface(Object, String)} can be called, which
     * automatically uses the {@link JavascriptInterface} annotation.
     * <li> Javascript interacts with Java objects on a private, background
     * thread of the WebContents. Care is therefore required to maintain
     * thread safety.</li>
     * </ul></p>
     *
     * @param object             The Java object to inject into the
     *                           WebContents's Javascript context. Null
     *                           values are ignored.
     * @param name               The name used to expose the instance in
     *                           Javascript.
     * @param requiredAnnotation Restrict exposed methods to ones with this
     *                           annotation.  If {@code null} all methods are
     *                           exposed.
     *
     */
    void addPossiblyUnsafeInterface(
            Object object, String name, Class<? extends Annotation> requiredAnnotation);

    /**
     * Removes a previously added Javascript interface with the given name.
     *
     * @param name The name of the interface to remove.
     */
    void removeInterface(String name);
}
