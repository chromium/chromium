// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {Persistence.MappingSystem}
 * @unrestricted
 */
Persistence.Automapping = class {
  /**
   * @param {!Workspace.Workspace} workspace
   * @param {function(!Persistence.AutomappingStatus)} onStatusAdded
   * @param {function(!Persistence.AutomappingStatus)} onStatusRemoved
   */
  constructor(workspace, onStatusAdded, onStatusRemoved) {
    this._workspace = workspace;

    this._onStatusAdded = onStatusAdded;
    this._onStatusRemoved = onStatusRemoved;
    /** @type {!Set<!Persistence.AutomappingStatus>} */
    this._statuses = new Set();

    /** @type {!Map<string, !Workspace.UISourceCode>} */
    this._fileSystemUISourceCodes = new Map();
    this._sweepThrottler = new Common.Throttler(100);

    const pathEncoder = new Persistence.PathEncoder();
    this._filesIndex = new Persistence.Automapping.FilePathIndex(pathEncoder);
    this._projectFoldersIndex = new Persistence.Automapping.FolderIndex(pathEncoder);
    this._activeFoldersIndex = new Persistence.Automapping.FolderIndex(pathEncoder);

    this._eventListeners = [
      this._workspace.addEventListener(
          Workspace.Workspace.Events.UISourceCodeAdded,
          event => this._onUISourceCodeAdded(/** @type {!Workspace.UISourceCode} */ (event.data))),
      this._workspace.addEventListener(
          Workspace.Workspace.Events.UISourceCodeRemoved,
          event => this._onUISourceCodeRemoved(/** @type {!Workspace.UISourceCode} */ (event.data))),
      this._workspace.addEventListener(
          Workspace.Workspace.Events.UISourceCodeRenamed, this._onUISourceCodeRenamed, this),
      this._workspace.addEventListener(
          Workspace.Workspace.Events.ProjectAdded,
          event => this._onProjectAdded(/** @type {!Workspace.Project} */ (event.data)), this),
      this._workspace.addEventListener(
          Workspace.Workspace.Events.ProjectRemoved,
          event => this._onProjectRemoved(/** @type {!Workspace.Project} */ (event.data)), this),
    ];

    for (const fileSystem of workspace.projects())
      this._onProjectAdded(fileSystem);
    for (const uiSourceCode of workspace.uiSourceCodes())
      this._onUISourceCodeAdded(uiSourceCode);
  }

  _scheduleRemap() {
    for (const status of this._statuses.valuesArray())
      this._clearNetworkStatus(status.network);
    this._scheduleSweep();
  }

  _scheduleSweep() {
    this._sweepThrottler.schedule(sweepUnmapped.bind(this));

    /**
     * @this {Persistence.Automapping}
     * @return {!Promise}
     */
    function sweepUnmapped() {
      const networkProjects = this._workspace.projectsForType(Workspace.projectTypes.Network);
      for (const networkProject of networkProjects) {
        for (const uiSourceCode of networkProject.uiSourceCodes())
          this._computeNetworkStatus(uiSourceCode);
      }
      this._onSweepHappenedForTest();
      return Promise.resolve();
    }
  }

  _onSweepHappenedForTest() {
  }

  /**
   * @param {!Workspace.Project} project
   */
  _onProjectRemoved(project) {
    for (const uiSourceCode of project.uiSourceCodes())
      this._onUISourceCodeRemoved(uiSourceCode);
    if (project.type() !== Workspace.projectTypes.FileSystem)
      return;
    const fileSystem = /** @type {!Persistence.FileSystemWorkspaceBinding.FileSystem} */ (project);
    for (const gitFolder of fileSystem.initialGitFolders())
      this._projectFoldersIndex.removeFolder(gitFolder);
    this._projectFoldersIndex.removeFolder(fileSystem.fileSystemPath());
    this._scheduleRemap();
  }

  /**
   * @param {!Workspace.Project} project
   */
  _onProjectAdded(project) {
    if (project.type() !== Workspace.projectTypes.FileSystem)
      return;
    const fileSystem = /** @type {!Persistence.FileSystemWorkspaceBinding.FileSystem} */ (project);
    for (const gitFolder of fileSystem.initialGitFolders())
      this._projectFoldersIndex.addFolder(gitFolder);
    this._projectFoldersIndex.addFolder(fileSystem.fileSystemPath());
    project.uiSourceCodes().forEach(this._onUISourceCodeAdded.bind(this));
    this._scheduleRemap();
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  _onUISourceCodeAdded(uiSourceCode) {
    const project = uiSourceCode.project();
    if (project.type() === Workspace.projectTypes.FileSystem) {
      if (!Persistence.FileSystemWorkspaceBinding.fileSystemSupportsAutomapping(project))
        return;
      this._filesIndex.addPath(uiSourceCode.url());
      this._fileSystemUISourceCodes.set(uiSourceCode.url(), uiSourceCode);
      this._scheduleSweep();
    } else if (project.type() === Workspace.projectTypes.Network) {
      this._computeNetworkStatus(uiSourceCode);
    }
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  _onUISourceCodeRemoved(uiSourceCode) {
    if (uiSourceCode.project().type() === Workspace.projectTypes.FileSystem) {
      this._filesIndex.removePath(uiSourceCode.url());
      this._fileSystemUISourceCodes.delete(uiSourceCode.url());
      const status = uiSourceCode[Persistence.Automapping._status];
      if (status)
        this._clearNetworkStatus(status.network);
    } else if (uiSourceCode.project().type() === Workspace.projectTypes.Network) {
      this._clearNetworkStatus(uiSourceCode);
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _onUISourceCodeRenamed(event) {
    const uiSourceCode = /** @type {!Workspace.UISourceCode} */ (event.data.uiSourceCode);
    const oldURL = /** @type {string} */ (event.data.oldURL);
    if (uiSourceCode.project().type() !== Workspace.projectTypes.FileSystem)
      return;

    this._filesIndex.removePath(oldURL);
    this._fileSystemUISourceCodes.delete(oldURL);
    const status = uiSourceCode[Persistence.Automapping._status];
    if (status)
      this._clearNetworkStatus(status.network);

    this._filesIndex.addPath(uiSourceCode.url());
    this._fileSystemUISourceCodes.set(uiSourceCode.url(), uiSourceCode);
    this._scheduleSweep();
  }

  /**
   * @param {!Workspace.UISourceCode} networkSourceCode
   */
  _computeNetworkStatus(networkSourceCode) {
    if (networkSourceCode[Persistence.Automapping._processingPromise] ||
        networkSourceCode[Persistence.Automapping._status])
      return;
    if (networkSourceCode.url().startsWith('wasm://'))
      return;
    const createBindingPromise =
        this._createBinding(networkSourceCode).then(validateStatus.bind(this)).then(onStatus.bind(this));
    networkSourceCode[Persistence.Automapping._processingPromise] = createBindingPromise;

    /**
     * @param {?Persistence.AutomappingStatus} status
     * @return {!Promise<?Persistence.AutomappingStatus>}
     * @this {Persistence.Automapping}
     */
    async function validateStatus(status) {
      if (!status)
        return null;
      if (networkSourceCode[Persistence.Automapping._processingPromise] !== createBindingPromise)
        return null;
      if (status.network.contentType().isFromSourceMap() || !status.fileSystem.contentType().isTextType())
        return status;

      // At the time binding comes, there are multiple user scenarios:
      // 1. Both network and fileSystem files are **not** dirty.
      //    This is a typical scenario when user hasn't done any edits yet to the
      //    files in question.
      // 2. FileSystem file has unsaved changes, network is clear.
      //    This typically happens with CSS files editing. Consider the following
      //    scenario:
      //      - user edits file that has been successfully mapped before
      //      - user doesn't save the file
      //      - user hits reload
      // 3. Network file has either unsaved changes or commits, but fileSystem file is clear.
      //    This typically happens when we've been editing file and then realized we'd like to drop
      //    a folder and persist all the changes.
      // 4. Network file has either unsaved changes or commits, and fileSystem file has unsaved changes.
      //    We consider this to be un-realistic scenario and in this case just fail gracefully.
      //
      // To support usecase (3), we need to validate against original network content.
      if (status.fileSystem.isDirty() && (status.network.isDirty() || status.network.hasCommits()))
        return null;

      const contents = await Promise.all([
        status.fileSystem.requestContent(),
        new Promise(x => status.network.project().requestFileContent(status.network, x))
      ]);
      const fileSystemContent = contents[0];
      const networkContent = contents[1];
      if (fileSystemContent === null || networkContent === null)
        return null;

      if (networkSourceCode[Persistence.Automapping._processingPromise] !== createBindingPromise)
        return null;

      const target = Bindings.NetworkProject.targetForUISourceCode(status.network);
      let isValid = false;
      if (target && target.isNodeJS()) {
        const rewrappedNetworkContent =
            Persistence.Persistence.rewrapNodeJSContent(status.fileSystem, fileSystemContent, networkContent);
        isValid = fileSystemContent === rewrappedNetworkContent;
      } else {
        // Trim trailing whitespaces because V8 adds trailing newline.
        isValid = fileSystemContent.trimRight() === networkContent.trimRight();
      }
      if (!isValid) {
        this._prevalidationFailedForTest(status);
        return null;
      }
      return status;
    }

    /**
     * @param {?Persistence.AutomappingStatus} status
     * @this {Persistence.Automapping}
     */
    function onStatus(status) {
      if (networkSourceCode[Persistence.Automapping._processingPromise] !== createBindingPromise)
        return;
      networkSourceCode[Persistence.Automapping._processingPromise] = null;
      if (!status || this._disposed) {
        this._onBindingFailedForTest();
        return;
      }
      // TODO(lushnikov): remove this check once there's a single uiSourceCode per url. @see crbug.com/670180
      if (status.network[Persistence.Automapping._status] || status.fileSystem[Persistence.Automapping._status])
        return;

      this._statuses.add(status);
      status.network[Persistence.Automapping._status] = status;
      status.fileSystem[Persistence.Automapping._status] = status;
      if (status.exactMatch) {
        const projectFolder = this._projectFoldersIndex.closestParentFolder(status.fileSystem.url());
        const newFolderAdded = projectFolder ? this._activeFoldersIndex.addFolder(projectFolder) : false;
        if (newFolderAdded)
          this._scheduleSweep();
      }
      this._onStatusAdded.call(null, status);
    }
  }

  /**
   * @param {!Persistence.AutomappingStatus} binding
   */
  _prevalidationFailedForTest(binding) {
  }

  _onBindingFailedForTest() {
  }

  /**
   * @param {!Workspace.UISourceCode} networkSourceCode
   */
  _clearNetworkStatus(networkSourceCode) {
    if (networkSourceCode[Persistence.Automapping._processingPromise]) {
      networkSourceCode[Persistence.Automapping._processingPromise] = null;
      return;
    }
    const status = networkSourceCode[Persistence.Automapping._status];
    if (!status)
      return;

    this._statuses.delete(status);
    status.network[Persistence.Automapping._status] = null;
    status.fileSystem[Persistence.Automapping._status] = null;
    if (status.exactMatch) {
      const projectFolder = this._projectFoldersIndex.closestParentFolder(status.fileSystem.url());
      if (projectFolder)
        this._activeFoldersIndex.removeFolder(projectFolder);
    }
    this._onStatusRemoved.call(null, status);
  }

  /**
   * @param {!Workspace.UISourceCode} networkSourceCode
   * @return {!Promise<?Persistence.AutomappingStatus>}
   */
  _createBinding(networkSourceCode) {
    if (networkSourceCode.url().startsWith('file://') || networkSourceCode.url().startsWith('snippet://')) {
      const fileSourceCode = this._fileSystemUISourceCodes.get(networkSourceCode.url());
      const status =
          fileSourceCode ? new Persistence.AutomappingStatus(networkSourceCode, fileSourceCode, false) : null;
      return Promise.resolve(status);
    }

    let networkPath = Common.ParsedURL.extractPath(networkSourceCode.url());
    if (networkPath === null)
      return Promise.resolve(/** @type {?Persistence.AutomappingStatus} */ (null));

    if (networkPath.endsWith('/'))
      networkPath += 'index.html';
    const similarFiles =
        this._filesIndex.similarFiles(networkPath).map(path => this._fileSystemUISourceCodes.get(path));
    if (!similarFiles.length)
      return Promise.resolve(/** @type {?Persistence.AutomappingStatus} */ (null));

    return this._pullMetadatas(similarFiles.concat(networkSourceCode)).then(onMetadatas.bind(this));

    /**
     * @this {Persistence.Automapping}
     */
    function onMetadatas() {
      const activeFiles = similarFiles.filter(file => !!this._activeFoldersIndex.closestParentFolder(file.url()));
      const networkMetadata = networkSourceCode[Persistence.Automapping._metadata];
      if (!networkMetadata || (!networkMetadata.modificationTime && typeof networkMetadata.contentSize !== 'number')) {
        // If networkSourceCode does not have metadata, try to match against active folders.
        if (activeFiles.length !== 1)
          return null;
        return new Persistence.AutomappingStatus(networkSourceCode, activeFiles[0], false);
      }

      // Try to find exact matches, prioritizing active folders.
      let exactMatches = this._filterWithMetadata(activeFiles, networkMetadata);
      if (!exactMatches.length)
        exactMatches = this._filterWithMetadata(similarFiles, networkMetadata);
      if (exactMatches.length !== 1)
        return null;
      return new Persistence.AutomappingStatus(networkSourceCode, exactMatches[0], true);
    }
  }

  /**
   * @param {!Array<!Workspace.UISourceCode>} uiSourceCodes
   * @return {!Promise}
   */
  _pullMetadatas(uiSourceCodes) {
    const promises = uiSourceCodes.map(file => fetchMetadata(file));
    return Promise.all(promises);

    /**
     * @param {!Workspace.UISourceCode} file
     * @return {!Promise}
     */
    function fetchMetadata(file) {
      return file.requestMetadata().then(metadata => file[Persistence.Automapping._metadata] = metadata);
    }
  }

  /**
   * @param {!Array<!Workspace.UISourceCode>} files
   * @param {!Workspace.UISourceCodeMetadata} networkMetadata
   * @return {!Array<!Workspace.UISourceCode>}
   */
  _filterWithMetadata(files, networkMetadata) {
    return files.filter(file => {
      const fileMetadata = file[Persistence.Automapping._metadata];
      if (!fileMetadata)
        return false;
      // Allow a second of difference due to network timestamps lack of precision.
      const timeMatches = !networkMetadata.modificationTime ||
          Math.abs(networkMetadata.modificationTime - fileMetadata.modificationTime) < 1000;
      const contentMatches = !networkMetadata.contentSize || fileMetadata.contentSize === networkMetadata.contentSize;
      return timeMatches && contentMatches;
    });
  }

  /**
   * @override
   */
  dispose() {
    if (this._disposed)
      return;
    this._disposed = true;
    Common.EventTarget.removeEventListeners(this._eventListeners);
    for (const status of this._statuses.valuesArray())
      this._clearNetworkStatus(status.network);
  }
};

Persistence.Automapping._status = Symbol('Automapping.Status');
Persistence.Automapping._processingPromise = Symbol('Automapping.ProcessingPromise');
Persistence.Automapping._metadata = Symbol('Automapping.Metadata');

/**
 * @unrestricted
 */
Persistence.Automapping.FilePathIndex = class {
  /**
   * @param {!Persistence.PathEncoder} encoder
   */
  constructor(encoder) {
    this._encoder = encoder;
    this._reversedIndex = new Common.Trie();
  }

  /**
   * @param {string} path
   */
  addPath(path) {
    const encodedPath = this._encoder.encode(path);
    this._reversedIndex.add(encodedPath.reverse());
  }

  /**
   * @param {string} path
   */
  removePath(path) {
    const encodedPath = this._encoder.encode(path);
    this._reversedIndex.remove(encodedPath.reverse());
  }

  /**
   * @param {string} networkPath
   * @return {!Array<string>}
   */
  similarFiles(networkPath) {
    const encodedPath = this._encoder.encode(networkPath);
    const longestCommonPrefix = this._reversedIndex.longestPrefix(encodedPath.reverse(), false);
    if (!longestCommonPrefix)
      return [];
    return this._reversedIndex.words(longestCommonPrefix)
        .map(encodedPath => this._encoder.decode(encodedPath.reverse()));
  }
};

/**
 * @unrestricted
 */
Persistence.Automapping.FolderIndex = class {
  /**
   * @param {!Persistence.PathEncoder} encoder
   */
  constructor(encoder) {
    this._encoder = encoder;
    this._index = new Common.Trie();
    /** @type {!Map<string, number>} */
    this._folderCount = new Map();
  }

  /**
   * @param {string} path
   * @return {boolean}
   */
  addFolder(path) {
    if (path.endsWith('/'))
      path = path.substring(0, path.length - 1);
    const encodedPath = this._encoder.encode(path);
    this._index.add(encodedPath);
    const count = this._folderCount.get(encodedPath) || 0;
    this._folderCount.set(encodedPath, count + 1);
    return count === 0;
  }

  /**
   * @param {string} path
   * @return {boolean}
   */
  removeFolder(path) {
    if (path.endsWith('/'))
      path = path.substring(0, path.length - 1);
    const encodedPath = this._encoder.encode(path);
    const count = this._folderCount.get(encodedPath) || 0;
    if (!count)
      return false;
    if (count > 1) {
      this._folderCount.set(encodedPath, count - 1);
      return false;
    }
    this._index.remove(encodedPath);
    this._folderCount.delete(encodedPath);
    return true;
  }

  /**
   * @param {string} path
   * @return {string}
   */
  closestParentFolder(path) {
    const encodedPath = this._encoder.encode(path);
    const commonPrefix = this._index.longestPrefix(encodedPath, true);
    return this._encoder.decode(commonPrefix);
  }
};

/**
 * @unrestricted
 */
Persistence.AutomappingStatus = class {
  /**
   * @param {!Workspace.UISourceCode} network
   * @param {!Workspace.UISourceCode} fileSystem
   * @param {boolean} exactMatch
   */
  constructor(network, fileSystem, exactMatch) {
    this.network = network;
    this.fileSystem = fileSystem;
    this.exactMatch = exactMatch;
  }
};
