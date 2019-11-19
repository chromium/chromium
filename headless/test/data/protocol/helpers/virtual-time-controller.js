// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to manage virtual time and automatically generate animation
 * frames within the granted virtual time interval.
 */
(class VirtualTimeController {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {?number} animationFrameInterval in milliseconds, integer.
   * @param {?number} maxTaskStarvationCount Specifies the maximum number of
   *     tasks that can be run before virtual time is forced forward to prevent
   *     deadlock.
   */
  constructor(testRunner, dp, animationFrameInterval, maxTaskStarvationCount) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.animationFrameInterval_ = animationFrameInterval || 16;
    this.maxTaskStarvationCount_ = maxTaskStarvationCount || 100 * 1000;
    this.virtualTimeBase_ = 0;
    this.remainingBudget_ = 0;
    this.lastGrantedChunk_ = 0;
    this.totalElapsedTime_ = 0;
    this.onInstalled_ = null;
    this.onExpired_ = null;

    this.dp_.Emulation.onVirtualTimeBudgetExpired(async data => {
      this.totalElapsedTime_ += this.lastGrantedChunk_;
      this.remainingBudget_ -= this.lastGrantedChunk_;
      if (this.remainingBudget_ === 0) {
        if (this.onExpired_) {
          this.onExpired_(this.totalElapsedTime_);
        }
      } else {
        await this.issueAnimationFrameAndScheduleNextChunk_();
      }
    });
  }

  /**
   * Grants initial portion of virtual time.
   * @param {number} budget Virtual time budget in milliseconds.
   * @param {number} initialVirtualTime Initial virtual time in milliseconds.
   * @param {?function()} onInstalled Called when initial virtual time is
   *     granted, parameter specifies virtual time base.
   * @param {?function()} onExpired Called when granted virtual time is expired,
   *     parameter specifies total elapsed virtual time.
   */
  async grantInitialTime(budget, initialVirtualTime, onInstalled, onExpired) {
    // Pause for the first time and remember base virtual time.
    this.virtualTimeBase_ = (await this.dp_.Emulation.setVirtualTimePolicy(
        {initialVirtualTime, policy: 'pause'}))
        .result.virtualTimeTicksBase;
    // Renderer wants the very first frame to be fully updated.
    await this.dp_.HeadlessExperimental.beginFrame({
        noDisplayUpdates: false,
        frameTimeTicks: this.virtualTimeBase_});

    this.onInstalled_ = onInstalled;
    await this.grantTime(budget, onExpired);
  }

  /**
   * Grants additional virtual time.
   * @param {number} budget Virtual time budget in milliseconds.
   * @param {?function()} onExpired Called when granted virtual time is expired,
   *     parameter specifies total elapsed virtual time.
   */
  async grantTime(budget, onExpired) {
    this.remainingBudget_ = budget;
    this.onExpired_ = onExpired;
    await this.issueAnimationFrameAndScheduleNextChunk_();
  }

  /**
   * Retrieves current frame time to be used in beginFrame calls.
   * @return {number} Frame time in milliseconds.
   */
  currentFrameTime() {
    return this.virtualTimeBase_ + this.totalElapsedTime_;
  }

  /**
   * Revokes any granted virtual time, resulting in no more animation frames
   * being issued and final OnExpired call being made.
   */
  stopVirtualTimeGracefully() {
    if (this.remainingBudget_) {
      this.remainingBudget_ = 0;
    }
  }

  /**
   * Capture screenshot of the entire screen and return a 2d graphics context
   * that has the resulting screenshot painted.
   */
  async captureScreenshot() {
    const frameTimeTicks = this.currentFrameTime();
    const screenshotData =
        (await this.dp_.HeadlessExperimental.beginFrame(
            {frameTimeTicks, screenshot: {format: 'png'}}))
        .result.screenshotData;
    // Advance virtual time a bit so that next frame timestamp is greater.
    this.virtualTimeBase_ += 0.01;
    const image = new Image();
    await new Promise(fulfill => {
      image.onload = fulfill;
      image.src = `data:image/png;base64,${screenshotData}`;
    });
    this.testRunner_.log(
        `Screenshot size: ${image.naturalWidth} x ${image.naturalHeight}`);
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(image, 0, 0);
    return ctx;
  }

  async issueAnimationFrameAndScheduleNextChunk_() {
    if (this.totalElapsedTime_ > 0 && this.remainingBudget_ > 0) {
      const remainder = this.totalElapsedTime_ % this.animationFrameInterval_;
      if (remainder === 0) {  // at the frame boundary?
        const frameTimeTicks = this.virtualTimeBase_ + this.totalElapsedTime_;
        await this.dp_.HeadlessExperimental.beginFrame(
            {frameTimeTicks, noDisplayUpdates: true});
      }
    }
    await this.scheduleNextChunk_();
  }

  async scheduleNextChunk_() {
    const lastFrame = this.totalElapsedTime_ % this.animationFrameInterval_;
    const nextAnimationFrame = this.animationFrameInterval_ - lastFrame;
    const chunk = Math.min(nextAnimationFrame, this.remainingBudget_);
    await this.dp_.Emulation.setVirtualTimePolicy(
        {policy: 'pauseIfNetworkFetchesPending', budget: chunk,
        maxVirtualTimeTaskStarvationCount: this.maxTaskStarvationCount_,
        waitForNavigation: this.totalElapsedTime_ === 0});
    this.lastGrantedChunk_ = chunk;

    if (this.onInstalled_) {
      this.onInstalled_(this.virtualTimeBase_);
      this.onInstalled_ = null;
    }
  }
});
