// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import static org.hamcrest.Matchers.isIn;
import static org.hamcrest.Matchers.not;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests the object registry, which maintains bidirectional object/ID mappings.
 */
@RunWith(BaseRobolectricTestRunner.class)
public final class RemoteObjectRegistryTest {
    @Test
    public void testMaintainsRetainingSet() {
        // This is how the registry is expected to keep itself alive, despite only being held weakly
        // from its main consumer.
        Set<RemoteObjectRegistry> retainingSet = new HashSet<>();
        RemoteObjectRegistry registry = new RemoteObjectRegistry(retainingSet);
        Assert.assertThat(registry, isIn(retainingSet));
        registry.close();
        Assert.assertThat(registry, not(isIn(retainingSet)));
    }

    @Test
    public void testGetObjectId() {
        // We should get an ID that can be used to retrieve the object.
        Set<RemoteObjectRegistry> retainingSet = new HashSet<>();
        RemoteObjectRegistry registry = new RemoteObjectRegistry(retainingSet);
        Object o = new Object();
        int id = registry.getObjectId(o, null);
        Assert.assertSame(o, registry.getObjectById(id));
    }

    @Test
    public void testGetObjectIdSame() {
        // The ID should be the same if retrieved twice.
        Set<RemoteObjectRegistry> retainingSet = new HashSet<>();
        RemoteObjectRegistry registry = new RemoteObjectRegistry(retainingSet);
        Object o = new Object();
        int id = registry.getObjectId(o, null);
        Assert.assertEquals(id, registry.getObjectId(o, null));
    }

    @Test
    public void testGetObjectIdAfterRemoval() {
        // It should still work if we have previously added and removed the object.
        Set<RemoteObjectRegistry> retainingSet = new HashSet<>();
        RemoteObjectRegistry registry = new RemoteObjectRegistry(retainingSet);
        Object o = new Object();
        int id = registry.getObjectId(o, null);
        registry.unrefObjectById(id);
        int id2 = registry.getObjectId(o, null);
        Assert.assertSame(o, registry.getObjectById(id2));
    }

    @Test
    public void testReturnsNullForNonExistentObject() {
        Set<RemoteObjectRegistry> retainingSet = new HashSet<>();
        RemoteObjectRegistry registry = new RemoteObjectRegistry(retainingSet);
        Assert.assertNull(registry.getObjectById(123));
    }
}
