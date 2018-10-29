/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @implements {Profiler.ProfileType.DataDisplayDelegate}
 * @unrestricted
 */
Profiler.ProfilesPanel = class extends UI.PanelWithSidebar {
  /**
   * @param {string} name
   * @param {!Array.<!Profiler.ProfileType>} profileTypes
   * @param {string} recordingActionId
   */
  constructor(name, profileTypes, recordingActionId) {
    super(name);
    this._profileTypes = profileTypes;
    this.registerRequiredCSS('profiler/heapProfiler.css');
    this.registerRequiredCSS('profiler/profilesPanel.css');
    this.registerRequiredCSS('object_ui/objectValue.css');

    const mainContainer = new UI.VBox();
    this.splitWidget().setMainWidget(mainContainer);

    this.profilesItemTreeElement = new Profiler.ProfilesSidebarTreeElement(this);

    this._sidebarTree = new UI.TreeOutlineInShadow();
    this._sidebarTree.registerRequiredCSS('profiler/profilesSidebarTree.css');
    this._sidebarTree.element.classList.add('profiles-sidebar-tree-box');
    this.panelSidebarElement().appendChild(this._sidebarTree.element);

    this._sidebarTree.appendChild(this.profilesItemTreeElement);

    this.profileViews = createElement('div');
    this.profileViews.id = 'profile-views';
    this.profileViews.classList.add('vbox');
    mainContainer.element.appendChild(this.profileViews);

    this._toolbarElement = createElementWithClass('div', 'profiles-toolbar');
    mainContainer.element.insertBefore(this._toolbarElement, mainContainer.element.firstChild);

    this.panelSidebarElement().classList.add('profiles-tree-sidebar');
    const toolbarContainerLeft = createElementWithClass('div', 'profiles-toolbar');
    this.panelSidebarElement().insertBefore(toolbarContainerLeft, this.panelSidebarElement().firstChild);
    const toolbar = new UI.Toolbar('', toolbarContainerLeft);

    this._toggleRecordAction =
        /** @type {!UI.Action }*/ (UI.actionRegistry.action(recordingActionId));
    this._toggleRecordButton = UI.Toolbar.createActionButton(this._toggleRecordAction);
    toolbar.appendToolbarItem(this._toggleRecordButton);

    this.clearResultsButton = new UI.ToolbarButton(Common.UIString('Clear all profiles'), 'largeicon-clear');
    this.clearResultsButton.addEventListener(UI.ToolbarButton.Events.Click, this._reset, this);
    toolbar.appendToolbarItem(this.clearResultsButton);
    toolbar.appendSeparator();
    toolbar.appendToolbarItem(UI.Toolbar.createActionButtonForId('components.collect-garbage'));

    this._profileViewToolbar = new UI.Toolbar('', this._toolbarElement);

    this._profileGroups = {};
    this._launcherView = new Profiler.ProfileLauncherView(this);
    this._launcherView.addEventListener(
        Profiler.ProfileLauncherView.Events.ProfileTypeSelected, this._onProfileTypeSelected, this);

    this._profileToView = [];
    this._typeIdToSidebarSection = {};
    const types = this._profileTypes;
    for (let i = 0; i < types.length; i++)
      this._registerProfileType(types[i]);
    this._launcherView.restoreSelectedProfileType();
    this.profilesItemTreeElement.select();
    this._showLauncherView();

    this._createFileSelectorElement();
    this.element.addEventListener('contextmenu', this._handleContextMenuEvent.bind(this), false);

    this.contentElement.addEventListener('keydown', this._onKeyDown.bind(this), false);

    SDK.targetManager.addEventListener(SDK.TargetManager.Events.SuspendStateChanged, this._onSuspendStateChanged, this);
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    let handled = false;
    if (event.key === 'ArrowDown' && !event.altKey)
      handled = this._sidebarTree.selectNext();
    else if (event.key === 'ArrowUp' && !event.altKey)
      handled = this._sidebarTree.selectPrevious();
    if (handled)
      event.consume(true);
  }

  /**
   * @override
   * @return {?UI.SearchableView}
   */
  searchableView() {
    return this.visibleView && this.visibleView.searchableView ? this.visibleView.searchableView() : null;
  }

  _createFileSelectorElement() {
    if (this._fileSelectorElement)
      this.element.removeChild(this._fileSelectorElement);
    this._fileSelectorElement = UI.createFileSelectorElement(this._loadFromFile.bind(this));
    Profiler.ProfilesPanel._fileSelectorElement = this._fileSelectorElement;
    this.element.appendChild(this._fileSelectorElement);
  }

  /**
   * @param {string} fileName
   * @return {?Profiler.ProfileType}
   */
  _findProfileTypeByExtension(fileName) {
    return this._profileTypes.find(type => !!type.fileExtension() && fileName.endsWith(type.fileExtension() || '')) ||
        null;
  }

  /**
   * @param {!File} file
   */
  async _loadFromFile(file) {
    this._createFileSelectorElement();

    const profileType = this._findProfileTypeByExtension(file.name);
    if (!profileType) {
      const extensions = new Set(this._profileTypes.map(type => type.fileExtension()).filter(ext => ext));
      Common.console.error(
          Common.UIString(`Can't load file. Supported file extensions: '%s'.`, Array.from(extensions).join(`', '`)));
      return;
    }

    if (!!profileType.profileBeingRecorded()) {
      Common.console.error(Common.UIString(`Can't load profile while another profile is being recorded.`));
      return;
    }

    const error = await profileType.loadFromFile(file);
    if (error)
      UI.MessageDialog.show(Common.UIString('Profile loading failed: %s.', error.message));
  }

  /**
   * @return {boolean}
   */
  toggleRecord() {
    if (!this._toggleRecordAction.enabled())
      return true;
    const type = this._selectedProfileType;
    const isProfiling = type.buttonClicked();
    this._updateToggleRecordAction(isProfiling);
    if (isProfiling) {
      this._launcherView.profileStarted();
      if (type.hasTemporaryView())
        this.showProfile(type.profileBeingRecorded());
    } else {
      this._launcherView.profileFinished();
    }
    return true;
  }

  _onSuspendStateChanged() {
    this._updateToggleRecordAction(this._toggleRecordAction.toggled());
  }

  /**
   * @param {boolean} toggled
   */
  _updateToggleRecordAction(toggled) {
    const enable = toggled || !SDK.targetManager.allTargetsSuspended();
    this._toggleRecordAction.setEnabled(enable);
    this._toggleRecordAction.setToggled(toggled);
    if (enable)
      this._toggleRecordButton.setTitle(this._selectedProfileType ? this._selectedProfileType.buttonTooltip : '');
    else
      this._toggleRecordButton.setTitle(UI.anotherProfilerActiveLabel());
    if (this._selectedProfileType)
      this._launcherView.updateProfileType(this._selectedProfileType, enable);
  }

  _profileBeingRecordedRemoved() {
    this._updateToggleRecordAction(false);
    this._launcherView.profileFinished();
  }

  /**
   * @param {!Common.Event} event
   */
  _onProfileTypeSelected(event) {
    this._selectedProfileType = /** @type {!Profiler.ProfileType} */ (event.data);
    this._updateProfileTypeSpecificUI();
  }

  _updateProfileTypeSpecificUI() {
    this._updateToggleRecordAction(this._toggleRecordAction.toggled());
  }

  _reset() {
    this._profileTypes.forEach(type => type.reset());

    delete this.visibleView;

    this._profileGroups = {};
    this._updateToggleRecordAction(false);
    this._launcherView.profileFinished();

    this._sidebarTree.element.classList.remove('some-expandable');

    this._launcherView.detach();
    this.profileViews.removeChildren();
    this._profileViewToolbar.removeToolbarItems();

    this.clearResultsButton.element.classList.remove('hidden');
    this.profilesItemTreeElement.select();
    this._showLauncherView();
  }

  _showLauncherView() {
    this.closeVisibleView();
    this._profileViewToolbar.removeToolbarItems();
    this._launcherView.show(this.profileViews);
    this.visibleView = this._launcherView;
    this._toolbarElement.classList.add('hidden');
  }

  /**
   * @param {!Profiler.ProfileType} profileType
   */
  _registerProfileType(profileType) {
    this._launcherView.addProfileType(profileType);
    const profileTypeSection = new Profiler.ProfileTypeSidebarSection(this, profileType);
    this._typeIdToSidebarSection[profileType.id] = profileTypeSection;
    this._sidebarTree.appendChild(profileTypeSection);
    profileTypeSection.childrenListElement.addEventListener(
        'contextmenu', this._handleContextMenuEvent.bind(this), false);

    /**
     * @param {!Common.Event} event
     * @this {Profiler.ProfilesPanel}
     */
    function onAddProfileHeader(event) {
      this._addProfileHeader(/** @type {!Profiler.ProfileHeader} */ (event.data));
    }

    /**
     * @param {!Common.Event} event
     * @this {Profiler.ProfilesPanel}
     */
    function onRemoveProfileHeader(event) {
      this._removeProfileHeader(/** @type {!Profiler.ProfileHeader} */ (event.data));
    }

    /**
     * @param {!Common.Event} event
     * @this {Profiler.ProfilesPanel}
     */
    function profileComplete(event) {
      this.showProfile(/** @type {!Profiler.ProfileHeader} */ (event.data));
    }

    profileType.addEventListener(Profiler.ProfileType.Events.ViewUpdated, this._updateProfileTypeSpecificUI, this);
    profileType.addEventListener(Profiler.ProfileType.Events.AddProfileHeader, onAddProfileHeader, this);
    profileType.addEventListener(Profiler.ProfileType.Events.RemoveProfileHeader, onRemoveProfileHeader, this);
    profileType.addEventListener(Profiler.ProfileType.Events.ProfileComplete, profileComplete, this);

    const profiles = profileType.getProfiles();
    for (let i = 0; i < profiles.length; i++)
      this._addProfileHeader(profiles[i]);
  }

  /**
   * @param {!Event} event
   */
  _handleContextMenuEvent(event) {
    const contextMenu = new UI.ContextMenu(event);
    if (this.panelSidebarElement().isSelfOrAncestor(event.srcElement)) {
      contextMenu.defaultSection().appendItem(
          Common.UIString('Load\u2026'), this._fileSelectorElement.click.bind(this._fileSelectorElement));
    }
    contextMenu.show();
  }

  showLoadFromFileDialog() {
    this._fileSelectorElement.click();
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   */
  _addProfileHeader(profile) {
    const profileType = profile.profileType();
    const typeId = profileType.id;
    this._typeIdToSidebarSection[typeId].addProfileHeader(profile);
    if (!this.visibleView || this.visibleView === this._launcherView)
      this.showProfile(profile);
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   */
  _removeProfileHeader(profile) {
    if (profile.profileType().profileBeingRecorded() === profile)
      this._profileBeingRecordedRemoved();

    const i = this._indexOfViewForProfile(profile);
    if (i !== -1)
      this._profileToView.splice(i, 1);

    const typeId = profile.profileType().id;
    const sectionIsEmpty = this._typeIdToSidebarSection[typeId].removeProfileHeader(profile);

    // No other item will be selected if there aren't any other profiles, so
    // make sure that view gets cleared when the last profile is removed.
    if (sectionIsEmpty) {
      this.profilesItemTreeElement.select();
      this._showLauncherView();
    }
  }

  /**
   * @override
   * @param {?Profiler.ProfileHeader} profile
   * @return {?UI.Widget}
   */
  showProfile(profile) {
    if (!profile ||
        (profile.profileType().profileBeingRecorded() === profile) && !profile.profileType().hasTemporaryView())
      return null;

    const view = this.viewForProfile(profile);
    if (view === this.visibleView)
      return view;

    this.closeVisibleView();

    view.show(this.profileViews);
    view.focus();
    this._toolbarElement.classList.remove('hidden');
    this.visibleView = view;

    const profileTypeSection = this._typeIdToSidebarSection[profile.profileType().id];
    const sidebarElement = profileTypeSection.sidebarElementForProfile(profile);
    sidebarElement.revealAndSelect();

    this._profileViewToolbar.removeToolbarItems();

    const toolbarItems = view.syncToolbarItems();
    for (let i = 0; i < toolbarItems.length; ++i)
      this._profileViewToolbar.appendToolbarItem(toolbarItems[i]);

    return view;
  }

  /**
   * @override
   * @param {!Protocol.HeapProfiler.HeapSnapshotObjectId} snapshotObjectId
   * @param {string} perspectiveName
   */
  showObject(snapshotObjectId, perspectiveName) {
  }

  /**
   * @override
   * @param {number} nodeIndex
   * @return {!Promise<?Element>}
   */
  async linkifyObject(nodeIndex) {
    return null;
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   * @return {!UI.Widget}
   */
  viewForProfile(profile) {
    const index = this._indexOfViewForProfile(profile);
    if (index !== -1)
      return this._profileToView[index].view;
    const view = profile.createView(this);
    view.element.classList.add('profile-view');
    this._profileToView.push({profile: profile, view: view});
    return view;
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   * @return {number}
   */
  _indexOfViewForProfile(profile) {
    return this._profileToView.findIndex(item => item.profile === profile);
  }

  closeVisibleView() {
    if (this.visibleView)
      this.visibleView.detach();
    delete this.visibleView;
  }

  /**
   * @override
   */
  focus() {
    this._sidebarTree.focus();
  }
};

/**
 * @unrestricted
 */
Profiler.ProfileTypeSidebarSection = class extends UI.TreeElement {
  /**
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @param {!Profiler.ProfileType} profileType
   */
  constructor(dataDisplayDelegate, profileType) {
    super(profileType.treeItemTitle.escapeHTML(), true);
    this.selectable = false;
    this._dataDisplayDelegate = dataDisplayDelegate;
    /** @type {!Array<!Profiler.ProfileSidebarTreeElement>} */
    this._profileTreeElements = [];
    /** @type {!Object<string, !Profiler.ProfileTypeSidebarSection.ProfileGroup>} */
    this._profileGroups = {};
    this.expand();
    this.hidden = true;
    this.setCollapsible(false);
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   */
  addProfileHeader(profile) {
    this.hidden = false;
    const profileType = profile.profileType();
    let sidebarParent = this;
    const profileTreeElement = profile.createSidebarTreeElement(this._dataDisplayDelegate);
    this._profileTreeElements.push(profileTreeElement);

    if (!profile.fromFile() && profileType.profileBeingRecorded() !== profile) {
      const profileTitle = profile.title;
      let group = this._profileGroups[profileTitle];
      if (!group) {
        group = new Profiler.ProfileTypeSidebarSection.ProfileGroup();
        this._profileGroups[profileTitle] = group;
      }
      group.profileSidebarTreeElements.push(profileTreeElement);

      const groupSize = group.profileSidebarTreeElements.length;
      if (groupSize === 2) {
        // Make a group UI.TreeElement now that there are 2 profiles.
        group.sidebarTreeElement =
            new Profiler.ProfileGroupSidebarTreeElement(this._dataDisplayDelegate, profile.title);

        const firstProfileTreeElement = group.profileSidebarTreeElements[0];
        // Insert at the same index for the first profile of the group.
        const index = this.children().indexOf(firstProfileTreeElement);
        this.insertChild(group.sidebarTreeElement, index);

        // Move the first profile to the group.
        const selected = firstProfileTreeElement.selected;
        this.removeChild(firstProfileTreeElement);
        group.sidebarTreeElement.appendChild(firstProfileTreeElement);
        if (selected)
          firstProfileTreeElement.revealAndSelect();

        firstProfileTreeElement.setSmall(true);
        firstProfileTreeElement.setMainTitle(Common.UIString('Run %d', 1));

        this.treeOutline.element.classList.add('some-expandable');
      }

      if (groupSize >= 2) {
        sidebarParent = group.sidebarTreeElement;
        profileTreeElement.setSmall(true);
        profileTreeElement.setMainTitle(Common.UIString('Run %d', groupSize));
      }
    }

    sidebarParent.appendChild(profileTreeElement);
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   * @return {boolean}
   */
  removeProfileHeader(profile) {
    const index = this._sidebarElementIndex(profile);
    if (index === -1)
      return false;
    const profileTreeElement = this._profileTreeElements[index];
    this._profileTreeElements.splice(index, 1);

    let sidebarParent = this;
    const group = this._profileGroups[profile.title];
    if (group) {
      const groupElements = group.profileSidebarTreeElements;
      groupElements.splice(groupElements.indexOf(profileTreeElement), 1);
      if (groupElements.length === 1) {
        // Move the last profile out of its group and remove the group.
        const pos = sidebarParent.children().indexOf(
            /** @type {!Profiler.ProfileGroupSidebarTreeElement} */ (group.sidebarTreeElement));
        group.sidebarTreeElement.removeChild(groupElements[0]);
        this.insertChild(groupElements[0], pos);
        groupElements[0].setSmall(false);
        groupElements[0].setMainTitle(profile.title);
        this.removeChild(group.sidebarTreeElement);
      }
      if (groupElements.length !== 0)
        sidebarParent = group.sidebarTreeElement;
    }
    sidebarParent.removeChild(profileTreeElement);
    profileTreeElement.dispose();

    if (this.childCount())
      return false;
    this.hidden = true;
    return true;
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   * @return {?Profiler.ProfileSidebarTreeElement}
   */
  sidebarElementForProfile(profile) {
    const index = this._sidebarElementIndex(profile);
    return index === -1 ? null : this._profileTreeElements[index];
  }

  /**
   * @param {!Profiler.ProfileHeader} profile
   * @return {number}
   */
  _sidebarElementIndex(profile) {
    const elements = this._profileTreeElements;
    for (let i = 0; i < elements.length; i++) {
      if (elements[i].profile === profile)
        return i;
    }
    return -1;
  }

  /**
   * @override
   */
  onattach() {
    this.listItemElement.classList.add('profiles-tree-section');
  }
};

/**
 * @unrestricted
 */
Profiler.ProfileTypeSidebarSection.ProfileGroup = class {
  constructor() {
    /** @type {!Array<!Profiler.ProfileSidebarTreeElement>} */
    this.profileSidebarTreeElements = [];
    /** @type {?Profiler.ProfileGroupSidebarTreeElement} */
    this.sidebarTreeElement = null;
  }
};

/**
 * @unrestricted
 */
Profiler.ProfileSidebarTreeElement = class extends UI.TreeElement {
  /**
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @param {!Profiler.ProfileHeader} profile
   * @param {string} className
   */
  constructor(dataDisplayDelegate, profile, className) {
    super('', false);
    this._iconElement = createElementWithClass('div', 'icon');
    this._titlesElement = createElementWithClass('div', 'titles no-subtitle');
    this._titleContainer = this._titlesElement.createChild('span', 'title-container');
    this._titleElement = this._titleContainer.createChild('span', 'title');
    this._subtitleElement = this._titlesElement.createChild('span', 'subtitle');

    this._titleElement.textContent = profile.title;
    this._className = className;
    this._small = false;
    this._dataDisplayDelegate = dataDisplayDelegate;
    this.profile = profile;
    profile.addEventListener(Profiler.ProfileHeader.Events.UpdateStatus, this._updateStatus, this);
    if (profile.canSaveToFile())
      this._createSaveLink();
    else
      profile.addEventListener(Profiler.ProfileHeader.Events.ProfileReceived, this._onProfileReceived, this);
  }

  _createSaveLink() {
    this._saveLinkElement = this._titleContainer.createChild('span', 'save-link');
    this._saveLinkElement.textContent = Common.UIString('Save');
    this._saveLinkElement.addEventListener('click', this._saveProfile.bind(this), false);
  }

  _onProfileReceived(event) {
    this._createSaveLink();
  }

  /**
   * @param {!Common.Event} event
   */
  _updateStatus(event) {
    const statusUpdate = event.data;
    if (statusUpdate.subtitle !== null) {
      this._subtitleElement.textContent = statusUpdate.subtitle || '';
      this._titlesElement.classList.toggle('no-subtitle', !statusUpdate.subtitle);
    }
    if (typeof statusUpdate.wait === 'boolean' && this.listItemElement)
      this.listItemElement.classList.toggle('wait', statusUpdate.wait);
  }

  /**
   * @override
   * @param {!Event} event
   * @return {boolean}
   */
  ondblclick(event) {
    if (!this._editing)
      this._startEditing(/** @type {!Element} */ (event.target));
    return false;
  }

  /**
   * @param {!Element} eventTarget
   */
  _startEditing(eventTarget) {
    const container = eventTarget.enclosingNodeOrSelfWithClass('title');
    if (!container)
      return;
    const config = new UI.InplaceEditor.Config(this._editingCommitted.bind(this), this._editingCancelled.bind(this));
    this._editing = UI.InplaceEditor.startEditing(container, config);
  }

  /**
   * @param {!Element} container
   * @param {string} newTitle
   */
  _editingCommitted(container, newTitle) {
    delete this._editing;
    this.profile.setTitle(newTitle);
  }

  _editingCancelled() {
    delete this._editing;
  }

  dispose() {
    this.profile.removeEventListener(Profiler.ProfileHeader.Events.UpdateStatus, this._updateStatus, this);
    this.profile.removeEventListener(Profiler.ProfileHeader.Events.ProfileReceived, this._onProfileReceived, this);
  }

  /**
   * @override
   * @return {boolean}
   */
  onselect() {
    this._dataDisplayDelegate.showProfile(this.profile);
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  ondelete() {
    this.profile.profileType().removeProfile(this.profile);
    return true;
  }

  /**
   * @override
   */
  onattach() {
    if (this._className)
      this.listItemElement.classList.add(this._className);
    if (this._small)
      this.listItemElement.classList.add('small');
    this.listItemElement.appendChildren(this._iconElement, this._titlesElement);
    this.listItemElement.addEventListener('contextmenu', this._handleContextMenuEvent.bind(this), true);
  }

  /**
   * @param {!Event} event
   */
  _handleContextMenuEvent(event) {
    const profile = this.profile;
    const contextMenu = new UI.ContextMenu(event);
    // FIXME: use context menu provider
    contextMenu.headerSection().appendItem(
        Common.UIString('Load\u2026'),
        Profiler.ProfilesPanel._fileSelectorElement.click.bind(Profiler.ProfilesPanel._fileSelectorElement));
    if (profile.canSaveToFile())
      contextMenu.saveSection().appendItem(Common.UIString('Save\u2026'), profile.saveToFile.bind(profile));
    contextMenu.footerSection().appendItem(Common.UIString('Delete'), this.ondelete.bind(this));
    contextMenu.show();
  }

  _saveProfile(event) {
    this.profile.saveToFile();
  }

  /**
   * @param {boolean} small
   */
  setSmall(small) {
    this._small = small;
    if (this.listItemElement)
      this.listItemElement.classList.toggle('small', this._small);
  }

  /**
   * @param {string} title
   */
  setMainTitle(title) {
    this._titleElement.textContent = title;
  }
};

/**
 * @unrestricted
 */
Profiler.ProfileGroupSidebarTreeElement = class extends UI.TreeElement {
  /**
   * @param {!Profiler.ProfileType.DataDisplayDelegate} dataDisplayDelegate
   * @param {string} title
   */
  constructor(dataDisplayDelegate, title) {
    super('', true);
    this.selectable = false;
    this._dataDisplayDelegate = dataDisplayDelegate;
    this._title = title;
    this.expand();
    this.toggleOnClick = true;
  }

  /**
   * @override
   * @return {boolean}
   */
  onselect() {
    const hasChildren = this.childCount() > 0;
    if (hasChildren)
      this._dataDisplayDelegate.showProfile(this.lastChild().profile);
    return hasChildren;
  }

  /**
   * @override
   */
  onattach() {
    this.listItemElement.classList.add('profile-group-sidebar-tree-item');
    this.listItemElement.createChild('div', 'icon');
    this.listItemElement.createChild('div', 'titles no-subtitle')
        .createChild('span', 'title-container')
        .createChild('span', 'title')
        .textContent = this._title;
  }
};

Profiler.ProfilesSidebarTreeElement = class extends UI.TreeElement {
  /**
   * @param {!Profiler.ProfilesPanel} panel
   */
  constructor(panel) {
    super('', false);
    this.selectable = true;
    this._panel = panel;
  }

  /**
   * @override
   * @return {boolean}
   */
  onselect() {
    this._panel._showLauncherView();
    return true;
  }

  /**
   * @override
   */
  onattach() {
    this.listItemElement.classList.add('profile-launcher-view-tree-item');
    this.listItemElement.createChild('div', 'icon');
    this.listItemElement.createChild('div', 'titles no-subtitle')
        .createChild('span', 'title-container')
        .createChild('span', 'title')
        .textContent = Common.UIString('Profiles');
  }
};

/**
 * @implements {UI.ActionDelegate}
 */
Profiler.JSProfilerPanel = class extends Profiler.ProfilesPanel {
  constructor() {
    const registry = Profiler.ProfileTypeRegistry.instance;
    super('js_profiler', [registry.cpuProfileType], 'profiler.js-toggle-recording');
  }

  /**
   * @override
   */
  wasShown() {
    UI.context.setFlavor(Profiler.JSProfilerPanel, this);
  }

  /**
   * @override
   */
  willHide() {
    UI.context.setFlavor(Profiler.JSProfilerPanel, null);
  }

  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    const panel = UI.context.flavor(Profiler.JSProfilerPanel);
    console.assert(panel && panel instanceof Profiler.JSProfilerPanel);
    panel.toggleRecord();
    return true;
  }
};
