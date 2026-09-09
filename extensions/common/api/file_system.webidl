// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary AcceptOption {
  // This is the optional text description for this option. If not present,
  // a description will be automatically generated; typically containing an
  // expanded list of valid extensions (e.g. "text/html" may expand to
  // "*.html, *.htm").
  DOMString description;

  // Mime-types to accept, e.g. "image/jpeg" or "audio/*". One of mimeTypes or
  // extensions must contain at least one valid element.
  sequence<DOMString> mimeTypes;

  // Extensions to accept, e.g. "jpg", "gif", "crx".
  sequence<DOMString> extensions;
};

enum ChooseEntryType {
  // Prompts the user to open an existing file and returns a FileEntry on
  // success. From Chrome 31 onwards, the FileEntry will be writable if the
  // application has the 'write' permission under 'fileSystem'; otherwise, the
  // FileEntry will be read-only.
  "openFile",

  // Prompts the user to open an existing file and returns a writable
  // FileEntry on success. Calls using this type will fail with a runtime
  // error if the application doesn't have the 'write' permission under
  // 'fileSystem'.
  "openWritableFile",

  // Prompts the user to open an existing file or a new file and returns a
  // writable FileEntry on success. Calls using this type will fail with a
  // runtime error if the application doesn't have the 'write' permission
  // under 'fileSystem'.
  "saveFile",

  // Prompts the user to open a directory and returns a DirectoryEntry on
  // success. Calls using this type will fail with a runtime error if the
  // application doesn't have the 'directory' permission under 'fileSystem'.
  // If the application has the 'write' permission under 'fileSystem', the
  // returned DirectoryEntry will be writable; otherwise it will be read-only.
  // New in Chrome 31.
  "openDirectory"
};

dictionary ChooseEntryOptions {
  // Type of the prompt to show. The default is 'openFile'.
  ChooseEntryType type;

  // The suggested file name that will be presented to the user as the
  // default name to save. This is optional and only applies to 'saveFile'.
  DOMString suggestedName;

  // The optional list of accept options for this file opener. Each option
  // will be presented as a unique group to the end-user.
  sequence<AcceptOption> accepts;

  // Whether to accept all file types, in addition to the options specified
  // in the accepts argument. The default is true. If the accepts field is
  // unset or contains no valid entries, this will always be reset to true.
  boolean acceptsAllTypes;

  // Whether to accept multiple files. This is only supported for openFile and
  // openWritableFile. The callback to chooseEntry will be called with a list
  // of entries if this is set to true. Otherwise it will be called with a
  // single Entry.
  boolean acceptsMultiple;
};

dictionary RequestFileSystemOptions {
  // The ID of the requested volume.
  required DOMString volumeId;

  // Whether the requested file system should be writable. The default is
  // read-only.
  boolean writable;
};

// Represents a mounted volume, which can be accessed via <code>chrome.
// fileSystem.requestFileSystem</code>.
dictionary Volume {
  required DOMString volumeId;
  required boolean writable;
};

// Event notifying about an inserted or a removed volume from the system.
dictionary VolumeListChangedEvent {
  required sequence<Volume> volumes;
};

[instanceOf=Entry]
typedef object Entry;

[instanceOf=FileEntry]
typedef object FileEntry;

[instanceOf=FileSystem]
typedef object DOMFileSystem;

callback EntryCallback = undefined (Entry entry);

callback EntriesCallback = undefined (
    optional Entry entry,
    optional sequence<FileEntry> fileEntries);

callback OnVolumeListChangedListener = undefined (
    VolumeListChangedEvent event);

interface OnVolumeListChangedEvent : ExtensionEvent {
  static undefined addListener(OnVolumeListChangedListener listener);
  static undefined removeListener(OnVolumeListChangedListener listener);
  static boolean hasListener(OnVolumeListChangedListener listener);
};

// Use the <code>chrome.fileSystem</code> API to create, read, navigate,
// and write to the user's local file system. With this API, Chrome Apps can
// read and write to a user-selected location. For example, a text editor app
// can use the API to read and write local documents. All failures are notified
// via chrome.runtime.lastError.
interface FileSystem {
  // Get the display path of an Entry object. The display path is based on
  // the full path of the file or directory on the local file system, but may
  // be made more readable for display purposes.
  // |PromiseValue|: displayPath
  static Promise<DOMString> getDisplayPath(Entry entry);

  // Get a writable Entry from another Entry. This call will fail with a
  // runtime error if the application does not have the 'write' permission
  // under 'fileSystem'. If entry is a DirectoryEntry, this call will fail if
  // the application does not have the 'directory' permission under
  // 'fileSystem'.
  static undefined getWritableEntry(
      Entry entry,
      EntryCallback callback);

  // Gets whether this Entry is writable or not.
  // |PromiseValue|: isWritable
  static Promise<boolean> isWritableEntry(Entry entry);

  // Ask the user to choose a file or directory.
  static undefined chooseEntry(
      optional ChooseEntryOptions options,
      EntriesCallback callback);

  // Returns the file entry with the given id if it can be restored. This call
  // will fail with a runtime error otherwise.
  static undefined restoreEntry(
      DOMString id,
      EntryCallback callback);

  // Returns whether the app has permission to restore the entry with the
  // given id.
  // |PromiseValue|: isRestorable
  static Promise<boolean> isRestorable(DOMString id);

  // Returns an id that can be passed to restoreEntry to regain access to a
  // given file entry. Only the 500 most recently used entries are retained,
  // where calls to retainEntry and restoreEntry count as use. If the app has
  // the 'retainEntries' permission under 'fileSystem', entries are retained
  // indefinitely. Otherwise, entries are retained only while the app is
  // running and across restarts.
  static DOMString retainEntry(Entry entry);

  // Requests access to a file system for a volume represented by <code>
  // options.volumeId</code>. If <code>options.writable</code> is set to true,
  // then the file system will be writable. Otherwise, it will be read-only.
  // The <code>writable</code> option requires the <code>
  // "fileSystem": {"write"}</code> permission in the manifest. Available to
  // kiosk apps running in kiosk session only. For manual-launch kiosk mode, a
  // confirmation dialog will be shown on top of the active app window.
  // In case of an error, <code>fileSystem</code> will be undefined, and
  // <code>chrome.runtime.lastError</code> will be set.
  // |PromiseValue|: fileSystem
  static Promise<DOMFileSystem?> requestFileSystem(
      RequestFileSystemOptions options);

  // Returns a list of volumes available for <code>requestFileSystem()</code>.
  // The <code>"fileSystem": {"requestFileSystem"}</code> manifest permission
  // is required. Available to kiosk apps running in the kiosk session only.
  // In case of an error, <code>volumes</code> will be undefined, and <code>
  // chrome.runtime.lastError</code> will be set.
  // |PromiseValue|: volumes
  static Promise<sequence<Volume>?> getVolumeList();

  // Called when a list of available volumes is changed.
  static attribute OnVolumeListChangedEvent onVolumeListChanged;
};

partial interface Browser {
  static attribute FileSystem fileSystem;
};
