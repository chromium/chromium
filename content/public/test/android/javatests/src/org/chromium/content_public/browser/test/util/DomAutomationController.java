// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * This class implements DomAutomationController.send by injecting Javascript into
 * a page and polling for data.
 */
public class DomAutomationController {
    private WebContents mWebContents;

    /**
     * Enables domAutomationController on the given webContents. Must be called after every
     * page load.
     */
    public void inject(WebContents webContents) throws TimeoutException {
        mWebContents = webContents;
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mWebContents,
                "window.domAutomationController = {"
                        + "  data_: [],"
                        + "  send: function(x) { this.data_.push(x) },"
                        + "  hasData: function() { return this.data_.length > 0 },"
                        + "  getData: function() { return this.data_.shift() }"
                        + "}");
    }

    /**
     * Waits until domAutomationController.send(value) has been called and returns value in JSON
     * format.
     */
    public String waitForResult(String failureReason) throws TimeoutException {
        assert mWebContents != null;
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String result =
                            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                    mWebContents, "domAutomationController.hasData()");
                    return result.equals("true");
                },
                failureReason);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mWebContents, "domAutomationController.getData()");
    }
}
