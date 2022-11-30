// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.task.TaskPriority;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.TaskTraitsExtensionDescriptor;
import org.chromium.content_public.browser.BrowserTaskExecutor;
import org.chromium.content_public.browser.BrowserTaskType;

/**
 * Provides the implementation needed in UiThreadTaskTraits.
 */
public class UiThreadTaskTraitsImpl {
    private static class Descriptor
            implements TaskTraitsExtensionDescriptor<UiThreadTaskTraitsImpl> {
        // Corresponds to content::BrowserTaskTraitsExtension.
        private static final byte EXTENSION_ID = 1;

        // Keep in sync with content::BrowserTaskTraitsExtension::Serialize.
        private static final byte TASK_TYPE = 1;
        private static final byte NESTING_INDEX = 2;

        @Override
        public int getId() {
            return EXTENSION_ID;
        }

        @Override
        public UiThreadTaskTraitsImpl fromSerializedData(byte[] data) {
            int taskType = data[TASK_TYPE];
            return new UiThreadTaskTraitsImpl().setTaskType(taskType);
        }

        @Override
        public byte[] toSerializedData(UiThreadTaskTraitsImpl extension) {
            byte extensionData[] = new byte[TaskTraits.EXTENSION_STORAGE_SIZE];

            // Note we don't specify the UI thread directly here because it's ID 0 and the array is
            // initialized to zero.

            // Similarly we don't specify BrowserTaskType.Default its ID is also 0.

            // TODO(crbug.com/876272) Remove this if possible.
            extensionData[NESTING_INDEX] = 1; // Allow the task to run in a nested RunLoop.
            extensionData[TASK_TYPE] = (byte) extension.mTaskType;
            return extensionData;
        }
    }

    public static final TaskTraitsExtensionDescriptor<UiThreadTaskTraitsImpl> DESCRIPTOR =
            new Descriptor();

    public static final TaskTraits DEFAULT =
            TaskTraits.USER_VISIBLE.withExtension(DESCRIPTOR, new UiThreadTaskTraitsImpl());
    // NOTE: Depending on browser configuration, the underlying C++ task executor executes bootstrap
    // tasks either in a dedicated high-priority task queue or in the default priority-based task
    // queues. While in the former case the priority of individual bootstrap tasks is ignored, in
    // the latter case it is used. It is thus important that these tasks have USER_BLOCKING priority
    // so that they are ordered correctly with C++ tasks of type kBootstrap in this latter case.
    // UPDATE: We have reverted Java bootstrap task traits back to having USER_VISIBLE priority
    // to determine whether changing them to have USER_BLOCKING priority caused a performance
    // regression.
    public static final TaskTraits BOOTSTRAP = TaskTraits.USER_VISIBLE.withExtension(
            DESCRIPTOR, new UiThreadTaskTraitsImpl().setTaskType(BrowserTaskType.BOOTSTRAP));
    public static final TaskTraits BEST_EFFORT = DEFAULT.taskPriority(TaskPriority.BEST_EFFORT);
    public static final TaskTraits USER_VISIBLE = DEFAULT.taskPriority(TaskPriority.USER_VISIBLE);
    public static final TaskTraits USER_BLOCKING = DEFAULT.taskPriority(TaskPriority.USER_BLOCKING);

    static {
        BrowserTaskExecutor.register();
    }

    private @BrowserTaskType int mTaskType;

    private UiThreadTaskTraitsImpl() {
        mTaskType = BrowserTaskType.DEFAULT;
    }

    @BrowserTaskType
    public int getTaskType() {
        return mTaskType;
    }

    private UiThreadTaskTraitsImpl setTaskType(@BrowserTaskType int taskType) {
        mTaskType = taskType;
        return this;
    }
}
