// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "storage")]
pub mod ffi {
    #[repr(i32)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
    // TODO(crbug.com/522247731): add linter to here to ensure that when this
    // enum is modified the C++ one is modified as well.
    pub enum FileSystemType {
        #[cxx_name = "kFileSystemTypeUnknown"]
        Unknown = -1,
        #[cxx_name = "kFileSystemTypeTemporary"]
        Temporary = 0,
        #[cxx_name = "kFileSystemTypePersistent"]
        Persistent = 1,
        #[cxx_name = "kFileSystemTypeIsolated"]
        Isolated = 2,
        #[cxx_name = "kFileSystemTypeExternal"]
        External = 3,
        #[cxx_name = "kFileSystemInternalTypeEnumStart"]
        InternalTypeEnumStart = 99,
        #[cxx_name = "kFileSystemTypeTest"]
        Test = 100,
        #[cxx_name = "kFileSystemTypeLocal"]
        Local = 101,
        #[cxx_name = "kFileSystemTypeDragged"]
        Dragged = 103,
        #[cxx_name = "kFileSystemTypeLocalMedia"]
        LocalMedia = 104,
        #[cxx_name = "kFileSystemTypeDeviceMedia"]
        DeviceMedia = 105,
        #[cxx_name = "kFileSystemTypeSyncable"]
        Syncable = 106,
        #[cxx_name = "kFileSystemTypeSyncableForInternalSync"]
        SyncableForInternalSync = 107,
        #[cxx_name = "kFileSystemTypeLocalForPlatformApp"]
        LocalForPlatformApp = 108,
        #[cxx_name = "kFileSystemTypeProvided"]
        Provided = 110,
        #[cxx_name = "kFileSystemTypeDeviceMediaAsFileStorage"]
        DeviceMediaAsFileStorage = 111,
        #[cxx_name = "kFileSystemTypeArcContent"]
        ArcContent = 112,
        #[cxx_name = "kFileSystemTypeArcDocumentsProvider"]
        ArcDocumentsProvider = 113,
        #[cxx_name = "kFileSystemTypeDriveFs"]
        DriveFs = 114,
        #[cxx_name = "kFileSystemTypeSmbFs"]
        SmbFs = 115,
        #[cxx_name = "kFileSystemTypeFuseBox"]
        FuseBox = 116,
        #[cxx_name = "kFileSystemTypeInMemory"]
        InMemory = 117,
        #[cxx_name = "kFileSystemInternalTypeEnumEnd"]
        InternalTypeEnumEnd = 118,
    }

    unsafe extern "C++" {
        include!("storage/common/file_system/file_system_types.h");
        type FileSystemType;
    }
}

// Re-export it for easier use
pub use ffi::FileSystemType;
