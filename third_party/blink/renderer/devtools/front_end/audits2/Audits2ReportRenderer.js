// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @override
 */
Audits2.ReportRenderer = class extends ReportRenderer {
  /**
   * @param {!Element} el Parent element to render the report into.
   * @param {!ReportRenderer.RunnerResultArtifacts=} artifacts
   */
  static addViewTraceButton(el, artifacts) {
    if (!artifacts || !artifacts.traces || !artifacts.traces.defaultPass)
      return;

    const defaultPassTrace = artifacts.traces.defaultPass;
    const timelineButton = UI.createTextButton(Common.UIString('View Trace'), onViewTraceClick, 'view-trace');
    el.querySelector('.lh-column').appendChild(timelineButton);
    return el;

    async function onViewTraceClick() {
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.Audits2ViewTrace);
      await UI.inspectorView.showPanel('timeline');
      Timeline.TimelinePanel.instance().loadFromEvents(defaultPassTrace.traceEvents);
    }
  }
};

class ReportUIFeatures {
  /**
   * @param {!ReportRenderer.ReportJSON} report
   */
  initFeatures(report) {
  }
}


Audits2.DetailsRenderer = class extends DetailsRenderer {
  /**
   * @param {!DOM} dom
   */
  constructor(dom) {
    super(dom);
    this._onLoadPromise = null;
  }

  /**
   * @override
   * @param {!DetailsRenderer.NodeDetailsJSON} item
   * @return {!Element}
   */
  renderNode(item) {
    const element = super.renderNode(item);
    this._replaceWithDeferredNodeBlock(element, item);
    return element;
  }

  /**
   * @param {!Element} origElement
   * @param {!DetailsRenderer.NodeDetailsJSON} detailsItem
   */
  async _replaceWithDeferredNodeBlock(origElement, detailsItem) {
    const mainTarget = SDK.targetManager.mainTarget();
    if (!this._onLoadPromise) {
      const resourceTreeModel = mainTarget.model(SDK.ResourceTreeModel);
      this._onLoadPromise = resourceTreeModel.once(SDK.ResourceTreeModel.Events.Load);
    }

    await this._onLoadPromise;

    const domModel = mainTarget.model(SDK.DOMModel);
    if (!detailsItem.path)
      return;

    const nodeId = await domModel.pushNodeByPathToFrontend(detailsItem.path);

    if (!nodeId)
      return;
    const node = domModel.nodeForId(nodeId);
    if (!node)
      return;

    const element =
        await Common.Linkifier.linkify(node, /** @type {!Common.Linkifier.Options} */ ({title: detailsItem.snippet}));
    origElement.title = '';
    origElement.textContent = '';
    origElement.appendChild(element);
  }
};
