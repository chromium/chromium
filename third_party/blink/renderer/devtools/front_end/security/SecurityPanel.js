// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @implements {SDK.SDKModelObserver<!Security.SecurityModel>}
 * @unrestricted
 */
Security.SecurityPanel = class extends UI.PanelWithSidebar {
  constructor() {
    super('security');

    this._mainView = new Security.SecurityMainView(this);

    const title = createElementWithClass('span', 'title');
    title.textContent = Common.UIString('Overview');
    this._sidebarMainViewElement = new Security.SecurityPanelSidebarTreeElement(
        title, this._setVisibleView.bind(this, this._mainView), 'security-main-view-sidebar-tree-item', 'lock-icon');
    this._sidebarTree = new Security.SecurityPanelSidebarTree(this._sidebarMainViewElement, this.showOrigin.bind(this));
    this.panelSidebarElement().appendChild(this._sidebarTree.element);

    /** @type {!Map<!Protocol.Network.LoaderId, !SDK.NetworkRequest>} */
    this._lastResponseReceivedForLoaderId = new Map();

    /** @type {!Map<!Security.SecurityPanel.Origin, !Security.SecurityPanel.OriginState>} */
    this._origins = new Map();

    /** @type {!Map<!Network.NetworkLogView.MixedContentFilterValues, number>} */
    this._filterRequestCounts = new Map();

    SDK.targetManager.observeModels(Security.SecurityModel, this);
  }

  /**
   * @return {!Security.SecurityPanel}
   */
  static _instance() {
    return /** @type {!Security.SecurityPanel} */ (self.runtime.sharedInstance(Security.SecurityPanel));
  }

  /**
   * @param {string} text
   * @param {string} origin
   * @return {!Element}
   */
  static createCertificateViewerButtonForOrigin(text, origin) {
    return UI.createTextButton(text, async e => {
      e.consume();
      const names = await SDK.multitargetNetworkManager.getCertificate(origin);
      InspectorFrontendHost.showCertificateViewer(names);
    }, 'origin-button');
  }

  /**
   * @param {string} text
   * @param {!Array<string>} names
   * @return {!Element}
   */
  static createCertificateViewerButtonForCert(text, names) {
    return UI.createTextButton(text, e => {
      e.consume();
      InspectorFrontendHost.showCertificateViewer(names);
    }, 'security-certificate-button');
  }

  /**
   * @param {string} url
   * @param {string} securityState
   * @return {!Element}
   */
  static createHighlightedUrl(url, securityState) {
    const schemeSeparator = '://';
    const index = url.indexOf(schemeSeparator);

    // If the separator is not found, just display the text without highlighting.
    if (index === -1) {
      const text = createElement('span', '');
      text.textContent = url;
      return text;
    }

    const highlightedUrl = createElement('span', 'url-text');

    const scheme = url.substr(0, index);
    const content = url.substr(index + schemeSeparator.length);
    highlightedUrl.createChild('span', 'url-scheme-' + securityState).textContent = scheme;
    highlightedUrl.createChild('span', 'url-scheme-separator').textContent = schemeSeparator;
    highlightedUrl.createChild('span').textContent = content;

    return highlightedUrl;
  }


  /**
   * @param {!Protocol.Security.SecurityState} securityState
   */
  setRanInsecureContentStyle(securityState) {
    this._ranInsecureContentStyle = securityState;
  }

  /**
   * @param {!Protocol.Security.SecurityState} securityState
   */
  setDisplayedInsecureContentStyle(securityState) {
    this._displayedInsecureContentStyle = securityState;
  }

  /**
   * @param {!Protocol.Security.SecurityState} newSecurityState
   * @param {boolean} schemeIsCryptographic
   * @param {!Array<!Protocol.Security.SecurityStateExplanation>} explanations
   * @param {?Protocol.Security.InsecureContentStatus} insecureContentStatus
   * @param {?string} summary
   */
  _updateSecurityState(newSecurityState, schemeIsCryptographic, explanations, insecureContentStatus, summary) {
    this._sidebarMainViewElement.setSecurityState(newSecurityState);
    this._mainView.updateSecurityState(
        newSecurityState, schemeIsCryptographic, explanations, insecureContentStatus, summary);
  }

  /**
   * @param {!Common.Event} event
   */
  _onSecurityStateChanged(event) {
    const data = /** @type {!Security.PageSecurityState} */ (event.data);
    const securityState = /** @type {!Protocol.Security.SecurityState} */ (data.securityState);
    const schemeIsCryptographic = /** @type {boolean} */ (data.schemeIsCryptographic);
    const explanations = /** @type {!Array<!Protocol.Security.SecurityStateExplanation>} */ (data.explanations);
    const insecureContentStatus = /** @type {?Protocol.Security.InsecureContentStatus} */ (data.insecureContentStatus);
    const summary = /** @type {?string} */ (data.summary);
    this._updateSecurityState(securityState, schemeIsCryptographic, explanations, insecureContentStatus, summary);
  }

  selectAndSwitchToMainView() {
    // The sidebar element will trigger displaying the main view. Rather than making a redundant call to display the main view, we rely on this.
    this._sidebarMainViewElement.select(true);
  }
  /**
   * @param {!Security.SecurityPanel.Origin} origin
   */
  showOrigin(origin) {
    const originState = this._origins.get(origin);
    if (!originState.originView)
      originState.originView = new Security.SecurityOriginView(this, origin, originState);

    this._setVisibleView(originState.originView);
  }

  /**
   * @override
   */
  wasShown() {
    super.wasShown();
    if (!this._visibleView)
      this.selectAndSwitchToMainView();
  }

  /**
   * @override
   */
  focus() {
    this._sidebarTree.focus();
  }

  /**
   * @param {!UI.VBox} view
   */
  _setVisibleView(view) {
    if (this._visibleView === view)
      return;

    if (this._visibleView)
      this._visibleView.detach();

    this._visibleView = view;

    if (view)
      this.splitWidget().setMainWidget(view);
  }

  /**
   * @param {!Common.Event} event
   */
  _onResponseReceived(event) {
    const request = /** @type {!SDK.NetworkRequest} */ (event.data);
    if (request.resourceType() === Common.resourceTypes.Document)
      this._lastResponseReceivedForLoaderId.set(request.loaderId, request);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  _processRequest(request) {
    const origin = Common.ParsedURL.extractOrigin(request.url());

    if (!origin) {
      // We don't handle resources like data: URIs. Most of them don't affect the lock icon.
      return;
    }

    let securityState = /** @type {!Protocol.Security.SecurityState} */ (request.securityState());

    if (request.mixedContentType === Protocol.Security.MixedContentType.Blockable && this._ranInsecureContentStyle)
      securityState = this._ranInsecureContentStyle;
    else if (
        request.mixedContentType === Protocol.Security.MixedContentType.OptionallyBlockable &&
        this._displayedInsecureContentStyle)
      securityState = this._displayedInsecureContentStyle;

    if (this._origins.has(origin)) {
      const originState = this._origins.get(origin);
      const oldSecurityState = originState.securityState;
      originState.securityState = this._securityStateMin(oldSecurityState, securityState);
      if (oldSecurityState !== originState.securityState) {
        const securityDetails = /** @type {?Protocol.Network.SecurityDetails} */ (request.securityDetails());
        if (securityDetails)
          originState.securityDetails = securityDetails;
        this._sidebarTree.updateOrigin(origin, securityState);
        if (originState.originView)
          originState.originView.setSecurityState(securityState);
      }
    } else {
      // This stores the first security details we see for an origin, but we should
      // eventually store a (deduplicated) list of all the different security
      // details we have seen. https://crbug.com/503170
      const originState = {};
      originState.securityState = securityState;

      const securityDetails = request.securityDetails();
      if (securityDetails)
        originState.securityDetails = securityDetails;

      originState.loadedFromCache = request.cached();

      this._origins.set(origin, originState);

      this._sidebarTree.addOrigin(origin, securityState);

      // Don't construct the origin view yet (let it happen lazily).
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _onRequestFinished(event) {
    const request = /** @type {!SDK.NetworkRequest} */ (event.data);
    this._updateFilterRequestCounts(request);
    this._processRequest(request);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  _updateFilterRequestCounts(request) {
    if (request.mixedContentType === Protocol.Security.MixedContentType.None)
      return;

    /** @type {!Network.NetworkLogView.MixedContentFilterValues} */
    let filterKey = Network.NetworkLogView.MixedContentFilterValues.All;
    if (request.wasBlocked())
      filterKey = Network.NetworkLogView.MixedContentFilterValues.Blocked;
    else if (request.mixedContentType === Protocol.Security.MixedContentType.Blockable)
      filterKey = Network.NetworkLogView.MixedContentFilterValues.BlockOverridden;
    else if (request.mixedContentType === Protocol.Security.MixedContentType.OptionallyBlockable)
      filterKey = Network.NetworkLogView.MixedContentFilterValues.Displayed;

    if (!this._filterRequestCounts.has(filterKey))
      this._filterRequestCounts.set(filterKey, 1);
    else
      this._filterRequestCounts.set(filterKey, this._filterRequestCounts.get(filterKey) + 1);

    this._mainView.refreshExplanations();
  }

  /**
   * @param {!Network.NetworkLogView.MixedContentFilterValues} filterKey
   * @return {number}
   */
  filterRequestCount(filterKey) {
    return this._filterRequestCounts.get(filterKey) || 0;
  }

  /**
   * @param {!Protocol.Security.SecurityState} stateA
   * @param {!Protocol.Security.SecurityState} stateB
   * @return {!Protocol.Security.SecurityState}
   */
  _securityStateMin(stateA, stateB) {
    return Security.SecurityModel.SecurityStateComparator(stateA, stateB) < 0 ? stateA : stateB;
  }

  /**
   * @override
   * @param {!Security.SecurityModel} securityModel
   */
  modelAdded(securityModel) {
    if (this._securityModel)
      return;

    this._securityModel = securityModel;
    const resourceTreeModel = securityModel.resourceTreeModel();
    const networkManager = securityModel.networkManager();
    this._eventListeners = [
      securityModel.addEventListener(
          Security.SecurityModel.Events.SecurityStateChanged, this._onSecurityStateChanged, this),
      resourceTreeModel.addEventListener(
          SDK.ResourceTreeModel.Events.MainFrameNavigated, this._onMainFrameNavigated, this),
      resourceTreeModel.addEventListener(
          SDK.ResourceTreeModel.Events.InterstitialShown, this._onInterstitialShown, this),
      resourceTreeModel.addEventListener(
          SDK.ResourceTreeModel.Events.InterstitialHidden, this._onInterstitialHidden, this),
      networkManager.addEventListener(SDK.NetworkManager.Events.ResponseReceived, this._onResponseReceived, this),
      networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, this._onRequestFinished, this),
    ];

    if (resourceTreeModel.isInterstitialShowing())
      this._onInterstitialShown();
  }

  /**
   * @override
   * @param {!Security.SecurityModel} securityModel
   */
  modelRemoved(securityModel) {
    if (this._securityModel !== securityModel)
      return;

    delete this._securityModel;
    Common.EventTarget.removeEventListeners(this._eventListeners);
  }

  /**
   * @param {!Common.Event} event
   */
  _onMainFrameNavigated(event) {
    const frame = /** type {!Protocol.Page.Frame}*/ (event.data);
    const request = this._lastResponseReceivedForLoaderId.get(frame.loaderId);

    this.selectAndSwitchToMainView();
    this._sidebarTree.clearOrigins();
    this._origins.clear();
    this._lastResponseReceivedForLoaderId.clear();
    this._filterRequestCounts.clear();
    // After clearing the filtered request counts, refresh the
    // explanations to reflect the new counts.
    this._mainView.refreshExplanations();

    // If we could not find a matching request (as in the case of clicking
    // through an interstitial, see https://crbug.com/669309), set the origin
    // based upon the url data from the MainFrameNavigated event itself.
    const origin = Common.ParsedURL.extractOrigin(request ? request.url() : frame.url);
    this._sidebarTree.setMainOrigin(origin);

    if (request)
      this._processRequest(request);
  }

  _onInterstitialShown() {
    // The panel might have been displaying the origin view on the
    // previously loaded page. When showing an interstitial, switch
    // back to the Overview view.
    this.selectAndSwitchToMainView();
    this._sidebarTree.toggleOriginsList(true /* hidden */);
  }

  _onInterstitialHidden() {
    this._sidebarTree.toggleOriginsList(false /* hidden */);
  }
};

/** @typedef {string} */
Security.SecurityPanel.Origin;

/**
 * @typedef {Object}
 * @property {!Protocol.Security.SecurityState} securityState
 * @property {?Protocol.Network.SecurityDetails} securityDetails
 * @property {?Promise<>} certificateDetailsPromise
 * @property {?bool} loadedFromCache
 * @property {?Security.SecurityOriginView} originView
 */
Security.SecurityPanel.OriginState;


/**
 * @unrestricted
 */
Security.SecurityPanelSidebarTree = class extends UI.TreeOutlineInShadow {
  /**
   * @param {!Security.SecurityPanelSidebarTreeElement} mainViewElement
   * @param {function(!Security.SecurityPanel.Origin)} showOriginInPanel
   */
  constructor(mainViewElement, showOriginInPanel) {
    super();
    this.registerRequiredCSS('security/sidebar.css');
    this.registerRequiredCSS('security/lockIcon.css');
    this.appendChild(mainViewElement);

    this._showOriginInPanel = showOriginInPanel;
    this._mainOrigin = null;

    /** @type {!Map<!Security.SecurityPanelSidebarTree.OriginGroupName, !UI.TreeElement>} */
    this._originGroups = new Map();

    for (const key in Security.SecurityPanelSidebarTree.OriginGroupName) {
      const originGroupName = Security.SecurityPanelSidebarTree.OriginGroupName[key];
      const originGroup = new UI.TreeElement(originGroupName, true);
      originGroup.selectable = false;
      originGroup.setCollapsible(false);
      originGroup.expand();
      originGroup.listItemElement.classList.add('security-sidebar-origins');
      this._originGroups.set(originGroupName, originGroup);
      this.appendChild(originGroup);
    }
    this._clearOriginGroups();

    // This message will be removed by clearOrigins() during the first new page load after the panel was opened.
    const mainViewReloadMessage = new UI.TreeElement(Common.UIString('Reload to view details'));
    mainViewReloadMessage.selectable = false;
    mainViewReloadMessage.listItemElement.classList.add('security-main-view-reload-message');
    this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.MainOrigin)
        .appendChild(mainViewReloadMessage);

    /** @type {!Map<!Security.SecurityPanel.Origin, !Security.SecurityPanelSidebarTreeElement>} */
    this._elementsByOrigin = new Map();
  }

  /**
   * @param {boolean} hidden
   */
  toggleOriginsList(hidden) {
    for (const key in Security.SecurityPanelSidebarTree.OriginGroupName) {
      const originGroupName = Security.SecurityPanelSidebarTree.OriginGroupName[key];
      const group = this._originGroups.get(originGroupName);
      if (group)
        group.hidden = hidden;
    }
  }

  /**
   * @param {!Security.SecurityPanel.Origin} origin
   * @param {!Protocol.Security.SecurityState} securityState
   */
  addOrigin(origin, securityState) {
    const originElement = new Security.SecurityPanelSidebarTreeElement(
        Security.SecurityPanel.createHighlightedUrl(origin, securityState), this._showOriginInPanel.bind(this, origin),
        'security-sidebar-tree-item', 'security-property');
    this._elementsByOrigin.set(origin, originElement);
    this.updateOrigin(origin, securityState);
  }

  /**
   * @param {!Security.SecurityPanel.Origin} origin
   */
  setMainOrigin(origin) {
    this._mainOrigin = origin;
  }

  /**
   * @param {!Security.SecurityPanel.Origin} origin
   * @param {!Protocol.Security.SecurityState} securityState
   */
  updateOrigin(origin, securityState) {
    const originElement =
        /** @type {!Security.SecurityPanelSidebarTreeElement} */ (this._elementsByOrigin.get(origin));
    originElement.setSecurityState(securityState);

    let newParent;
    if (origin === this._mainOrigin) {
      newParent = this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.MainOrigin);
    } else {
      switch (securityState) {
        case Protocol.Security.SecurityState.Secure:
          newParent = this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.Secure);
          break;
        case Protocol.Security.SecurityState.Unknown:
          newParent = this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.Unknown);
          break;
        default:
          newParent = this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.NonSecure);
          break;
      }
    }

    const oldParent = originElement.parent;
    if (oldParent !== newParent) {
      if (oldParent) {
        oldParent.removeChild(originElement);
        if (oldParent.childCount() === 0)
          oldParent.hidden = true;
      }
      newParent.appendChild(originElement);
      newParent.hidden = false;
    }
  }

  _clearOriginGroups() {
    for (const originGroup of this._originGroups.values()) {
      originGroup.removeChildren();
      originGroup.hidden = true;
    }
    this._originGroups.get(Security.SecurityPanelSidebarTree.OriginGroupName.MainOrigin).hidden = false;
  }

  clearOrigins() {
    this._clearOriginGroups();
    this._elementsByOrigin.clear();
  }
};

/**
 * A mapping from Javascript key IDs to names (sidebar section titles).
 * Note: The names are used as keys into a map, so they must be distinct from each other.
 * @enum {string}
 */
Security.SecurityPanelSidebarTree.OriginGroupName = {
  MainOrigin: Common.UIString('Main origin'),
  NonSecure: Common.UIString('Non-secure origins'),
  Secure: Common.UIString('Secure origins'),
  Unknown: Common.UIString('Unknown / canceled')
};

/**
 * @unrestricted
 */
Security.SecurityPanelSidebarTreeElement = class extends UI.TreeElement {
  /**
   * @param {!Element} textElement
   * @param {function()} selectCallback
   * @param {string} className
   * @param {string} cssPrefix
   */
  constructor(textElement, selectCallback, className, cssPrefix) {
    super('', false);
    this._selectCallback = selectCallback;
    this._cssPrefix = cssPrefix;
    this.listItemElement.classList.add(className);
    this._iconElement = this.listItemElement.createChild('div', 'icon');
    this._iconElement.classList.add(this._cssPrefix);
    this.listItemElement.appendChild(textElement);
    this.setSecurityState(Protocol.Security.SecurityState.Unknown);
  }

  /**
   * @param {!Security.SecurityPanelSidebarTreeElement} a
   * @param {!Security.SecurityPanelSidebarTreeElement} b
   * @return {number}
   */
  static SecurityStateComparator(a, b) {
    return Security.SecurityModel.SecurityStateComparator(a.securityState(), b.securityState());
  }

  /**
   * @param {!Protocol.Security.SecurityState} newSecurityState
   */
  setSecurityState(newSecurityState) {
    if (this._securityState)
      this._iconElement.classList.remove(this._cssPrefix + '-' + this._securityState);

    this._securityState = newSecurityState;
    this._iconElement.classList.add(this._cssPrefix + '-' + newSecurityState);
  }

  /**
   * @return {!Protocol.Security.SecurityState}
   */
  securityState() {
    return this._securityState;
  }

  /**
   * @override
   * @return {boolean}
   */
  onselect() {
    this._selectCallback();
    return true;
  }
};


/**
 * @unrestricted
 */
Security.SecurityMainView = class extends UI.VBox {
  /**
   * @param {!Security.SecurityPanel} panel
   */
  constructor(panel) {
    super(true);
    this.registerRequiredCSS('security/mainView.css');
    this.registerRequiredCSS('security/lockIcon.css');
    this.setMinimumSize(200, 100);

    this.contentElement.classList.add('security-main-view');

    this._panel = panel;

    this._summarySection = this.contentElement.createChild('div', 'security-summary');

    // Info explanations should appear after all others.
    this._securityExplanationsMain =
        this.contentElement.createChild('div', 'security-explanation-list security-explanations-main');
    this._securityExplanationsExtra =
        this.contentElement.createChild('div', 'security-explanation-list security-explanations-extra');

    // Fill the security summary section.
    this._summarySection.createChild('div', 'security-summary-section-title').textContent =
        Common.UIString('Security overview');

    const lockSpectrum = this._summarySection.createChild('div', 'lock-spectrum');
    lockSpectrum.createChild('div', 'lock-icon lock-icon-secure').title = Common.UIString('Secure');
    lockSpectrum.createChild('div', 'lock-icon lock-icon-neutral').title = Common.UIString('Not secure');
    lockSpectrum.createChild('div', 'lock-icon lock-icon-insecure').title = Common.UIString('Not secure (broken)');

    this._summarySection.createChild('div', 'triangle-pointer-container')
        .createChild('div', 'triangle-pointer-wrapper')
        .createChild('div', 'triangle-pointer');

    this._summaryText = this._summarySection.createChild('div', 'security-summary-text');
  }

  /**
   * @param {!Element} parent
   * @param {!Protocol.Security.SecurityStateExplanation} explanation
   * @return {!Element}
   */
  _addExplanation(parent, explanation) {
    const explanationSection = parent.createChild('div', 'security-explanation');
    explanationSection.classList.add('security-explanation-' + explanation.securityState);

    explanationSection.createChild('div', 'security-property')
        .classList.add('security-property-' + explanation.securityState);
    const text = explanationSection.createChild('div', 'security-explanation-text');

    const explanationHeader = text.createChild('div', 'security-explanation-title');

    if (explanation.title) {
      explanationHeader.createChild('span').textContent = explanation.title + ' - ';
      explanationHeader.createChild('span', 'security-explanation-title-' + explanation.securityState).textContent =
          explanation.summary;
    } else {
      explanationHeader.textContent = explanation.summary;
    }

    text.createChild('div').textContent = explanation.description;

    if (explanation.certificate.length) {
      text.appendChild(Security.SecurityPanel.createCertificateViewerButtonForCert(
          Common.UIString('View certificate'), explanation.certificate));
    }

    if (explanation.recommendations && explanation.recommendations.length) {
      const recommendationList = text.createChild('ul', 'security-explanation-recommendations');
      for (const recommendation of explanation.recommendations)
        recommendationList.createChild('li').textContent = recommendation;
    }
    return text;
  }

  /**
   * @param {!Protocol.Security.SecurityState} newSecurityState
   * @param {boolean} schemeIsCryptographic
   * @param {!Array<!Protocol.Security.SecurityStateExplanation>} explanations
   * @param {?Protocol.Security.InsecureContentStatus} insecureContentStatus
   * @param {?string} summary
   */
  updateSecurityState(newSecurityState, schemeIsCryptographic, explanations, insecureContentStatus, summary) {
    // Remove old state.
    // It's safe to call this even when this._securityState is undefined.
    this._summarySection.classList.remove('security-summary-' + this._securityState);

    // Add new state.
    this._securityState = newSecurityState;
    this._summarySection.classList.add('security-summary-' + this._securityState);
    const summaryExplanationStrings = {
      'unknown': Common.UIString('The security of this page is unknown.'),
      'insecure': Common.UIString('This page is not secure (broken HTTPS).'),
      'neutral': Common.UIString('This page is not secure.'),
      'secure': Common.UIString('This page is secure (valid HTTPS).')
    };

    // Use override summary if present, otherwise use base explanation
    this._summaryText.textContent = summary || summaryExplanationStrings[this._securityState];

    this._explanations = explanations, this._insecureContentStatus = insecureContentStatus;
    this._schemeIsCryptographic = schemeIsCryptographic;

    this._panel.setRanInsecureContentStyle(insecureContentStatus.ranInsecureContentStyle);
    this._panel.setDisplayedInsecureContentStyle(insecureContentStatus.displayedInsecureContentStyle);

    this.refreshExplanations();
  }

  refreshExplanations() {
    this._securityExplanationsMain.removeChildren();
    this._securityExplanationsExtra.removeChildren();
    for (const explanation of this._explanations) {
      if (explanation.securityState === Protocol.Security.SecurityState.Info) {
        this._addExplanation(this._securityExplanationsExtra, explanation);
      } else {
        switch (explanation.mixedContentType) {
          case Protocol.Security.MixedContentType.Blockable:
            this._addMixedContentExplanation(
                this._securityExplanationsMain, explanation,
                Network.NetworkLogView.MixedContentFilterValues.BlockOverridden);
            break;
          case Protocol.Security.MixedContentType.OptionallyBlockable:
            this._addMixedContentExplanation(
                this._securityExplanationsMain, explanation, Network.NetworkLogView.MixedContentFilterValues.Displayed);
            break;
          default:
            this._addExplanation(this._securityExplanationsMain, explanation);
            break;
        }
      }
    }

    if (this._panel.filterRequestCount(Network.NetworkLogView.MixedContentFilterValues.Blocked) > 0) {
      const explanation = /** @type {!Protocol.Security.SecurityStateExplanation} */ ({
        securityState: Protocol.Security.SecurityState.Info,
        summary: Common.UIString('Blocked mixed content'),
        description: Common.UIString('Your page requested non-secure resources that were blocked.'),
        mixedContentType: Protocol.Security.MixedContentType.Blockable,
        certificate: []
      });
      this._addMixedContentExplanation(
          this._securityExplanationsMain, explanation, Network.NetworkLogView.MixedContentFilterValues.Blocked);
    }
  }

  /**
   * @param {!Element} parent
   * @param {!Protocol.Security.SecurityStateExplanation} explanation
   * @param {!Network.NetworkLogView.MixedContentFilterValues} filterKey
   */
  _addMixedContentExplanation(parent, explanation, filterKey) {
    const element = this._addExplanation(parent, explanation);

    const filterRequestCount = this._panel.filterRequestCount(filterKey);
    if (!filterRequestCount) {
      // Network instrumentation might not have been enabled for the page
      // load, so the security panel does not necessarily know a count of
      // individual mixed requests at this point. Prompt them to refresh
      // instead of pointing them to the Network panel to get prompted
      // to refresh.
      const refreshPrompt = element.createChild('div', 'security-mixed-content');
      refreshPrompt.textContent = Common.UIString('Reload the page to record requests for HTTP resources.');
      return;
    }

    const requestsAnchor = element.createChild('div', 'security-mixed-content link');
    if (filterRequestCount === 1)
      requestsAnchor.textContent = Common.UIString('View %d request in Network Panel', filterRequestCount);
    else
      requestsAnchor.textContent = Common.UIString('View %d requests in Network Panel', filterRequestCount);

    requestsAnchor.href = '';
    requestsAnchor.addEventListener('click', this.showNetworkFilter.bind(this, filterKey));
  }

  /**
   * @param {!Network.NetworkLogView.MixedContentFilterValues} filterKey
   * @param {!Event} e
   */
  showNetworkFilter(filterKey, e) {
    e.consume();
    Network.NetworkPanel.revealAndFilter(
        [{filterType: Network.NetworkLogView.FilterType.MixedContent, filterValue: filterKey}]);
  }
};

/**
 * @unrestricted
 */
Security.SecurityOriginView = class extends UI.VBox {
  /**
   * @param {!Security.SecurityPanel} panel
   * @param {!Security.SecurityPanel.Origin} origin
   * @param {!Security.SecurityPanel.OriginState} originState
   */
  constructor(panel, origin, originState) {
    super();
    this._panel = panel;
    this.setMinimumSize(200, 100);

    this.element.classList.add('security-origin-view');
    this.registerRequiredCSS('security/originView.css');
    this.registerRequiredCSS('security/lockIcon.css');

    const titleSection = this.element.createChild('div', 'title-section');
    titleSection.createChild('div', 'title-section-header').textContent = ls`Origin`;

    const originDisplay = titleSection.createChild('div', 'origin-display');
    this._originLockIcon = originDisplay.createChild('span', 'security-property');
    this._originLockIcon.classList.add('security-property-' + originState.securityState);

    originDisplay.appendChild(Security.SecurityPanel.createHighlightedUrl(origin, originState.securityState));

    const originNetworkButton = titleSection.createChild('div', 'view-network-button');
    originNetworkButton.appendChild(UI.createTextButton('View requests in Network Panel', e => {
      e.consume();
      const parsedURL = new Common.ParsedURL(origin);
      Network.NetworkPanel.revealAndFilter([
        {filterType: Network.NetworkLogView.FilterType.Domain, filterValue: parsedURL.host},
        {filterType: Network.NetworkLogView.FilterType.Scheme, filterValue: parsedURL.scheme}
      ]);
    }, 'origin-button'));

    if (originState.securityDetails) {
      const connectionSection = this.element.createChild('div', 'origin-view-section');
      connectionSection.createChild('div', 'origin-view-section-title').textContent = Common.UIString('Connection');

      let table = new Security.SecurityDetailsTable();
      connectionSection.appendChild(table.element());
      table.addRow(Common.UIString('Protocol'), originState.securityDetails.protocol);
      if (originState.securityDetails.keyExchange)
        table.addRow(Common.UIString('Key exchange'), originState.securityDetails.keyExchange);
      if (originState.securityDetails.keyExchangeGroup)
        table.addRow(Common.UIString('Key exchange group'), originState.securityDetails.keyExchangeGroup);
      table.addRow(
          Common.UIString('Cipher'),
          originState.securityDetails.cipher +
              (originState.securityDetails.mac ? ' with ' + originState.securityDetails.mac : ''));

      // Create the certificate section outside the callback, so that it appears in the right place.
      const certificateSection = this.element.createChild('div', 'origin-view-section');
      certificateSection.createChild('div', 'origin-view-section-title').textContent = Common.UIString('Certificate');

      const sctListLength = originState.securityDetails.signedCertificateTimestampList.length;
      const ctCompliance = originState.securityDetails.certificateTransparencyCompliance;
      let sctSection;
      if (sctListLength || ctCompliance !== Protocol.Network.CertificateTransparencyCompliance.Unknown) {
        // Create the Certificate Transparency section outside the callback, so that it appears in the right place.
        sctSection = this.element.createChild('div', 'origin-view-section');
        sctSection.createChild('div', 'origin-view-section-title').textContent =
            Common.UIString('Certificate Transparency');
      }

      const sanDiv = this._createSanDiv(originState.securityDetails.sanList);
      const validFromString = new Date(1000 * originState.securityDetails.validFrom).toUTCString();
      const validUntilString = new Date(1000 * originState.securityDetails.validTo).toUTCString();

      table = new Security.SecurityDetailsTable();
      certificateSection.appendChild(table.element());
      table.addRow(Common.UIString('Subject'), originState.securityDetails.subjectName);
      table.addRow(Common.UIString('SAN'), sanDiv);
      table.addRow(Common.UIString('Valid from'), validFromString);
      table.addRow(Common.UIString('Valid until'), validUntilString);
      table.addRow(Common.UIString('Issuer'), originState.securityDetails.issuer);

      table.addRow(
          '',
          Security.SecurityPanel.createCertificateViewerButtonForOrigin(
              Common.UIString('Open full certificate details'), origin));

      if (!sctSection)
        return;

      // Show summary of SCT(s) of Certificate Transparency.
      const sctSummaryTable = new Security.SecurityDetailsTable();
      sctSummaryTable.element().classList.add('sct-summary');
      sctSection.appendChild(sctSummaryTable.element());
      for (let i = 0; i < sctListLength; i++) {
        const sct = originState.securityDetails.signedCertificateTimestampList[i];
        sctSummaryTable.addRow(
            Common.UIString('SCT'), sct.logDescription + ' (' + sct.origin + ', ' + sct.status + ')');
      }

      // Show detailed SCT(s) of Certificate Transparency.
      const sctTableWrapper = sctSection.createChild('div', 'sct-details');
      sctTableWrapper.classList.add('hidden');
      for (let i = 0; i < sctListLength; i++) {
        const sctTable = new Security.SecurityDetailsTable();
        sctTableWrapper.appendChild(sctTable.element());
        const sct = originState.securityDetails.signedCertificateTimestampList[i];
        sctTable.addRow(Common.UIString('Log name'), sct.logDescription);
        sctTable.addRow(Common.UIString('Log ID'), sct.logId.replace(/(.{2})/g, '$1 '));
        sctTable.addRow(Common.UIString('Validation status'), sct.status);
        sctTable.addRow(Common.UIString('Source'), sct.origin);
        sctTable.addRow(Common.UIString('Issued at'), new Date(sct.timestamp).toUTCString());
        sctTable.addRow(Common.UIString('Hash algorithm'), sct.hashAlgorithm);
        sctTable.addRow(Common.UIString('Signature algorithm'), sct.signatureAlgorithm);
        sctTable.addRow(Common.UIString('Signature data'), sct.signatureData.replace(/(.{2})/g, '$1 '));
      }

      // Add link to toggle between displaying of the summary of the SCT(s) and the detailed SCT(s).
      if (sctListLength) {
        const toggleSctsDetailsLink = sctSection.createChild('div', 'link');
        toggleSctsDetailsLink.classList.add('sct-toggle');
        toggleSctsDetailsLink.textContent = Common.UIString('Show full details');
        function toggleSctDetailsDisplay() {
          const isDetailsShown = !sctTableWrapper.classList.contains('hidden');
          if (isDetailsShown)
            toggleSctsDetailsLink.textContent = Common.UIString('Show full details');
          else
            toggleSctsDetailsLink.textContent = Common.UIString('Hide full details');
          sctSummaryTable.element().classList.toggle('hidden');
          sctTableWrapper.classList.toggle('hidden');
        }
        toggleSctsDetailsLink.addEventListener('click', toggleSctDetailsDisplay, false);
      }

      switch (ctCompliance) {
        case Protocol.Network.CertificateTransparencyCompliance.Compliant:
          sctSection.createChild('div', 'origin-view-section-notes').textContent =
              Common.UIString('This request complies with Chrome\'s Certificate Transparency policy.');
          break;
        case Protocol.Network.CertificateTransparencyCompliance.NotCompliant:
          sctSection.createChild('div', 'origin-view-section-notes').textContent =
              Common.UIString('This request does not comply with Chrome\'s Certificate Transparency policy.');
          break;
        case Protocol.Network.CertificateTransparencyCompliance.Unknown:
          break;
      }

      const noteSection = this.element.createChild('div', 'origin-view-section origin-view-notes');
      if (originState.loadedFromCache) {
        noteSection.createChild('div').textContent =
            Common.UIString('This response was loaded from cache. Some security details might be missing.');
      }
      noteSection.createChild('div').textContent =
          Common.UIString('The security details above are from the first inspected response.');
    } else if (originState.securityState !== Protocol.Security.SecurityState.Unknown) {
      const notSecureSection = this.element.createChild('div', 'origin-view-section');
      notSecureSection.createChild('div', 'origin-view-section-title').textContent = Common.UIString('Not secure');
      notSecureSection.createChild('div').textContent =
          Common.UIString('Your connection to this origin is not secure.');
    } else {
      const noInfoSection = this.element.createChild('div', 'origin-view-section');
      noInfoSection.createChild('div', 'origin-view-section-title').textContent =
          Common.UIString('No security information');
      noInfoSection.createChild('div').textContent =
          Common.UIString('No security details are available for this origin.');
    }
  }

  /**
   * @param {!Array<string>} sanList
   * *return {!Element}
   */
  _createSanDiv(sanList) {
    const sanDiv = createElement('div');
    if (sanList.length === 0) {
      sanDiv.textContent = Common.UIString('(n/a)');
      sanDiv.classList.add('empty-san');
    } else {
      const truncatedNumToShow = 2;
      const listIsTruncated = sanList.length > truncatedNumToShow + 1;
      for (let i = 0; i < sanList.length; i++) {
        const span = sanDiv.createChild('span', 'san-entry');
        span.textContent = sanList[i];
        if (listIsTruncated && i >= truncatedNumToShow)
          span.classList.add('truncated-entry');
      }
      if (listIsTruncated) {
        const truncatedSANToggle = sanDiv.createChild('div', 'link');
        truncatedSANToggle.href = '';

        function toggleSANTruncation() {
          if (sanDiv.classList.contains('truncated-san')) {
            sanDiv.classList.remove('truncated-san');
            truncatedSANToggle.textContent = Common.UIString('Show less');
          } else {
            sanDiv.classList.add('truncated-san');
            truncatedSANToggle.textContent = Common.UIString('Show more (%d total)', sanList.length);
          }
        }
        truncatedSANToggle.addEventListener('click', toggleSANTruncation, false);
        toggleSANTruncation();
      }
    }
    return sanDiv;
  }

  /**
   * @param {!Protocol.Security.SecurityState} newSecurityState
   */
  setSecurityState(newSecurityState) {
    for (const className of Array.prototype.slice.call(this._originLockIcon.classList)) {
      if (className.startsWith('security-property-'))
        this._originLockIcon.classList.remove(className);
    }

    this._originLockIcon.classList.add('security-property-' + newSecurityState);
  }
};

/**
 * @unrestricted
 */
Security.SecurityDetailsTable = class {
  constructor() {
    this._element = createElement('table');
    this._element.classList.add('details-table');
  }

  /**
   * @return: {!Element}
   */
  element() {
    return this._element;
  }

  /**
   * @param {string} key
   * @param {string|!Node} value
   */
  addRow(key, value) {
    const row = this._element.createChild('div', 'details-table-row');
    row.createChild('div').textContent = key;

    const valueDiv = row.createChild('div');
    if (typeof value === 'string')
      valueDiv.textContent = value;
    else
      valueDiv.appendChild(value);
  }
};
