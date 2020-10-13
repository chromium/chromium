// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import android.util.SparseArray;

import java.lang.annotation.Annotation;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Set;

/**
 * Owns a set of objects on behalf of RemoteObjectHost's client.
 *
 * These objects could contain references which would keep the WebContents alive
 * longer than expected, and so must not be held alive by any other GC root.
 */
final class RemoteObjectRegistry implements RemoteObjectImpl.ObjectIdAllocator {
    private final Set<? super RemoteObjectRegistry> mRetainingSet;
    private static class Entry {
        Entry(int id, Object object, Class<? extends Annotation> safeAnnotationClass) {
            this.id = id;
            this.object = object;
            this.safeAnnotationClass = safeAnnotationClass;
        }

        // The ID assigned to the object, which can be used by the renderer
        // to refer to it.
        public final int id;

        // The injected object itself
        public final Object object;

        // The number of outstanding references to this object.
        // This includes:
        //   * wrapper objects in the renderer (removed when it closes the pipe)
        //   * names assigned to the object by developer Java code
        public int referenceCount = 1;

        // The annotation class annotated to the object, which can be exposed
        // to JavaScript code.
        public Class<? extends Annotation> safeAnnotationClass;
    }

    private final SparseArray<Entry> mEntriesById = new SparseArray<>();
    private final Map<Object, Entry> mEntriesByObject = new IdentityHashMap<>();
    private int mNextId;

    RemoteObjectRegistry(Set<? super RemoteObjectRegistry> retainingSet) {
        retainingSet.add(this);
        mRetainingSet = retainingSet;
    }

    public void close() {
        boolean removed = mRetainingSet.remove(this);
        assert removed;
    }

    public synchronized Class<? extends Annotation> getSafeAnnotationClass(Object object) {
        Entry entry = mEntriesByObject.get(object);
        return entry != null ? entry.safeAnnotationClass : null;
    }

    @Override
    public synchronized int getObjectId(
            Object object, Class<? extends Annotation> safeAnnotationClass) {
        Entry entry = mEntriesByObject.get(object);
        if (entry != null) {
            entry.referenceCount++;
            return entry.id;
        }

        int newId = mNextId++;
        assert newId >= 0;
        entry = new Entry(newId, object, safeAnnotationClass);
        mEntriesById.put(newId, entry);
        mEntriesByObject.put(object, entry);
        return newId;
    }

    @Override
    public synchronized Object getObjectById(int id) {
        Entry entry = mEntriesById.get(id);
        return entry != null ? entry.object : null;
    }

    public synchronized void refObjectById(int id) {
        Entry entry = mEntriesById.get(id);
        if (entry == null) return;

        entry.referenceCount++;
    }

    public synchronized void unrefObjectById(int id) {
        unrefObject(mEntriesById.get(id));
    }

    public synchronized void unrefObjectByObject(Object object) {
        unrefObject(mEntriesByObject.get(object));
    }

    private synchronized void unrefObject(Entry entry) {
        if (entry == null) return;

        entry.referenceCount--;
        assert entry.referenceCount >= 0;
        if (entry.referenceCount > 0) return;

        mEntriesById.remove(entry.id);
        mEntriesByObject.remove(entry.object);
    }
}
