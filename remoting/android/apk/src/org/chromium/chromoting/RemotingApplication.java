// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chromoting.accountswitcher.AccountSwitcherFactory;
import org.chromium.chromoting.help.HelpAndFeedbackBasic;
import org.chromium.chromoting.help.HelpSingleton;
import org.chromium.chromoting.jni.JniInterface;

/** Main context for the application. */
public class RemotingApplication extends Application {
    @Override
    public void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);
        JniInterface.loadLibrary();
        AccountSwitcherFactory.setInstance(new AccountSwitcherFactory());
        HelpSingleton.setInstance(new HelpAndFeedbackBasic());
    }
}
