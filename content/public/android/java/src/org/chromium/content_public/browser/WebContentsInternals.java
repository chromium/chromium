// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * Marker interface for WebContents internal objects that should be managed by
 * embedders. Embedders should call {@link WebContents#setInternalHolder} with
 * an implementation of the interface {@link WebContents#InterfaceHolder} to
 * get the instance from {@link WebContents}. They don't have to know what's
 * inside the instance. This is necessary only for WebView so far, in order to
 * address the requirements that there not be any gc root to webview in content
 * layer after webview gets detached from view tree. See https://crbug.com/755174.
 */
public interface WebContentsInternals {}
