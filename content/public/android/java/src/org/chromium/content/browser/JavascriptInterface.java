// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Marks a method as being able to be exposed to JavaScript.  This is used for safety purposes so
 * that only explicitly marked methods get exposed instead of every method in a class.
 * @see {@link WebContents#addJavascriptInterface(Class, String, boolean)}
 */
@SuppressWarnings("javadoc")
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD})
public @interface JavascriptInterface {}
