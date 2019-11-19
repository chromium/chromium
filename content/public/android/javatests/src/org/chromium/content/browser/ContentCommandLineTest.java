// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_shell_apk.ContentShellApplication;

/**
 * Test class for command lines.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentCommandLineTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    // A reference command line. Note that switch2 is [brea\d], switch3 is [and "butter"],
    // and switch4 is [a "quoted" 'food'!]
    static final String INIT_SWITCHES[] = { "init_command", "--switch", "Arg",
        "--switch2=brea\\d", "--switch3=and \"butter\"",
        "--switch4=a \"quoted\" 'food'!",
        "--", "--actually_an_arg" };

    // The same command line, but in quoted string format.
    static final char INIT_SWITCHES_BUFFER[] =
        ("init_command --switch Arg --switch2=brea\\d --switch3=\"and \\\"butt\"er\\\"   "
        + "--switch4='a \"quoted\" \\'food\\'!' "
        + "-- --actually_an_arg").toCharArray();

    static final String CL_ADDED_SWITCH = "zappo-dappo-doggy-trainer";
    static final String CL_ADDED_SWITCH_2 = "username";
    static final String CL_ADDED_VALUE_2 = "bozo";

    @Before
    public void setUp() {
        CommandLine.reset();
    }

    void loadJni() {
        Assert.assertFalse(CommandLine.getInstance().isNativeImplementation());
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
        Assert.assertTrue(CommandLine.getInstance().isNativeImplementation());
    }

    void checkInitSwitches() {
        CommandLine cl = CommandLine.getInstance();
        Assert.assertFalse(cl.hasSwitch("init_command"));
        Assert.assertTrue(cl.hasSwitch("switch"));
        Assert.assertFalse(cl.hasSwitch("--switch"));
        Assert.assertFalse(cl.hasSwitch("arg"));
        Assert.assertFalse(cl.hasSwitch("actually_an_arg"));
        Assert.assertEquals("brea\\d", cl.getSwitchValue("switch2"));
        Assert.assertEquals("and \"butter\"", cl.getSwitchValue("switch3"));
        Assert.assertEquals("a \"quoted\" 'food'!", cl.getSwitchValue("switch4"));
        Assert.assertNull(cl.getSwitchValue("switch"));
        Assert.assertNull(cl.getSwitchValue("non-existant"));
    }

    void checkSettingThenGetting() {
        CommandLine cl = CommandLine.getInstance();

        // Add a plain switch.
        Assert.assertFalse(cl.hasSwitch(CL_ADDED_SWITCH));
        cl.appendSwitch(CL_ADDED_SWITCH);
        Assert.assertTrue(cl.hasSwitch(CL_ADDED_SWITCH));

        // Add a switch paired with a value.
        Assert.assertFalse(cl.hasSwitch(CL_ADDED_SWITCH_2));
        Assert.assertNull(cl.getSwitchValue(CL_ADDED_SWITCH_2));
        cl.appendSwitchWithValue(CL_ADDED_SWITCH_2, CL_ADDED_VALUE_2);
        Assert.assertTrue(CL_ADDED_VALUE_2.equals(cl.getSwitchValue(CL_ADDED_SWITCH_2)));

        // Append a few new things.
        final String switchesAndArgs[] = { "dummy", "--superfast", "--speed=turbo" };
        Assert.assertFalse(cl.hasSwitch("dummy"));
        Assert.assertFalse(cl.hasSwitch("superfast"));
        Assert.assertNull(cl.getSwitchValue("speed"));
        cl.appendSwitchesAndArguments(switchesAndArgs);
        Assert.assertFalse(cl.hasSwitch("dummy"));
        Assert.assertFalse(cl.hasSwitch("command"));
        Assert.assertTrue(cl.hasSwitch("superfast"));
        Assert.assertTrue("turbo".equals(cl.getSwitchValue("speed")));
    }

    void checkAppendedSwitchesPassedThrough() {
        CommandLine cl = CommandLine.getInstance();
        Assert.assertTrue(cl.hasSwitch(CL_ADDED_SWITCH));
        Assert.assertTrue(cl.hasSwitch(CL_ADDED_SWITCH_2));
        Assert.assertTrue(CL_ADDED_VALUE_2.equals(cl.getSwitchValue(CL_ADDED_SWITCH_2)));
    }

    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testJavaNativeTransition() {
        CommandLine.init(INIT_SWITCHES);
        checkInitSwitches();
        loadJni();
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testJavaNativeTransitionAfterAppends() {
        CommandLine.init(INIT_SWITCHES);
        checkInitSwitches();
        checkSettingThenGetting();
        loadJni();
        checkInitSwitches();
        checkAppendedSwitchesPassedThrough();
    }

    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNativeInitialization() {
        CommandLine.init(null);
        loadJni();
        // Drop the program name for use with appendSwitchesAndArguments.
        String[] args = new String[INIT_SWITCHES.length - 1];
        System.arraycopy(INIT_SWITCHES, 1, args, 0, args.length);
        CommandLine.getInstance().appendSwitchesAndArguments(args);
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testFileInitialization() {
        CommandLine.initFromFile(ContentShellApplication.COMMAND_LINE_FILE);
        loadJni();
        checkSettingThenGetting();
    }
}
