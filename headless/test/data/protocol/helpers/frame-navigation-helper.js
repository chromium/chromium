// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to keep track of renderer tests frame navigations.
 */
(class FrameNavigationHelper {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {!Page} page TestRunner.Page instance.
   */
  constructor(testRunner, dp, page) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.page_ = page;
    this.frames_ = new Map();
    this.scheduledNavigations_ = new Map();
    this.state_ = 'INIT';
    this.mainFrameId_ = null;
  }

  /**
   * Navigates to the specified url.
   *
   * @param {!string} Url to navigate to.
   */
  async navigate(url) {
    this.state_ = 'STARTING';
    await this.dp_.Page.navigate({url});
  }

  /**
   * Initializes the helper returning reference to itself to allow assignment.
   *
   * @return {!object} FrameNavigationHelper reference.
   */
  async init() {
    await this.dp_.Page.enable();

    await this.dp_.Page.onFrameStartedLoading(event => {
      if (this.state_ === 'STARTING') {
        this.state_ = 'LOADING';
        this.mainFrameId_ = event.params.frameId;
      }
    });

    await this.dp_.Page.onLoadEventFired(event => {
      if (this.state_ === 'STARTING' || this.state_ === 'LOADING') {
        this.state_ = 'RENDERING';
      }
    });

    await this.dp_.Page.onFrameNavigated(event => {
      const frameId = event.params.frame.id;
      let value = this.frames_.get(frameId) || [];
      value.push(event.params.frame);
      this.frames_.set(frameId, value);
    });

    await this.dp_.Page.onFrameScheduledNavigation(event => {
      const frameId = event.params.frameId;
      const reason = event.params.reason;
      const url = event.params.url;
      let value = this.scheduledNavigations_.get(frameId) || [];
      value.push({url, reason});
      this.scheduledNavigations_.set(frameId, value);
    });

    return this;
  }

  /**
   * Logs navigated frames.
   */
  logFrames() {
    this.testRunner_.log(`Frames: ${this.frames_.size}`);
    for (const [frameId, frames] of this.frames_.entries()) {
      this.testRunner_.log(` frameId=${this.getFrameId_(frameId)}`);
      for (const frame of frames) {
        this.testRunner_.log(`  url=${frame.url}`);
      }
    }
  }

  /**
   * Logs scheduled navigations.
   */
  logScheduledNavigations() {
    this.testRunner_.log(
        `ScheduledNavigations: ${this.scheduledNavigations_.size}`);
    for (const [frameId, navs] of this.scheduledNavigations_.entries()) {
      this.testRunner_.log(` frameId=${this.getFrameId_(frameId)}`);
      for (const nav of navs) {
        this.testRunner_.log(`  url=${nav.url} reason=${nav.reason}`);
      }
    }
  }

  getFrameId_(frameId) {
    return frameId === this.mainFrameId_ ? 'MainFrame' : `<${typeof frameId}>`
  }

});
