// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.blink.mojom.MessagePortDescriptor;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.Connector;
import org.chromium.mojo.system.Pair;

/**
 * Test suite for AppWebMessagePortDescriptor.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AppWebMessagePortDescriptorTest {
    @Rule
    public MojoTestRule mActivityTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    @Test
    @SmallTest
    public void testPassingAndEntangling() throws Throwable {
        Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> pair =
                AppWebMessagePortDescriptor.createPair();
        Assert.assertTrue(pair.first.isValid());
        Assert.assertFalse(pair.first.isEntangled());
        Assert.assertTrue(pair.second.isValid());
        Assert.assertFalse(pair.second.isEntangled());

        AppWebMessagePortDescriptor port0 = pair.first;
        Assert.assertTrue(port0.isValid());
        Assert.assertFalse(port0.isEntangled());

        AppWebMessagePortDescriptor port1 = pair.second;
        Assert.assertTrue(port1.isValid());
        Assert.assertFalse(port1.isEntangled());

        // Do a round trip through entanglement/disentanglement.

        Connector connector = port0.entangleWithConnector();
        Assert.assertTrue(port0.isValid());
        Assert.assertTrue(port0.isEntangled());

        port0.disentangleFromConnector();
        connector = null;
        Assert.assertTrue(port0.isValid());
        Assert.assertFalse(port0.isEntangled());

        // Do a round trip through serialization as a blink.mojom.MessagePortDescriptor.

        MessagePortDescriptor blinkPort = port0.passAsBlinkMojomMessagePortDescriptor();
        Assert.assertFalse(port0.isValid());
        Assert.assertFalse(port0.isEntangled());

        port0 = new AppWebMessagePortDescriptor(blinkPort);
        Assert.assertTrue(port0.isValid());
        Assert.assertFalse(port0.isEntangled());

        // Close the ports to satisfy lifetime assertions.

        port0.close();
        Assert.assertFalse(port0.isValid());
        Assert.assertFalse(port0.isEntangled());
        Assert.assertFalse(pair.first.isValid());
        Assert.assertFalse(pair.first.isEntangled());

        port1.close();
        Assert.assertFalse(port1.isValid());
        Assert.assertFalse(port1.isEntangled());
        Assert.assertFalse(pair.second.isValid());
        Assert.assertFalse(pair.second.isEntangled());
    }
}
