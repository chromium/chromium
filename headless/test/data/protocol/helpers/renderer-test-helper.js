// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to instantiate classes commonly used in renderer tests.
 */
(class RendererTestHelper {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {!Page} page TestRunner.Page instance.
   */
  constructor(testRunner, dp, page) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.page_ = page;
  }

  /**
   * Initializes the helper returning references to useful objects.
   *
   * @return {!object} httpInterceptor, frameNavigationHelper and
   *     virtualTimeController references.
   */
  async init() {
    await this.dp_.Page.enable();

    let HttpInterceptor = await this.testRunner_.loadScript(
        '../helpers/http-interceptor.js');
    let httpInterceptor =
        await (new HttpInterceptor(this.testRunner_, this.dp_))
        .init();

    let FrameNavigationHelper = await this.testRunner_.loadScript(
        '../helpers/frame-navigation-helper.js');
    let frameNavigationHelper =
        await (new FrameNavigationHelper(
            this.testRunner_, this.dp_, this.page_))
        .init();

    let VirtualTimeController = await this.testRunner_.loadScript(
        '../helpers/virtual-time-controller.js');
    let virtualTimeController =
        new VirtualTimeController(this.testRunner_, this.dp_, 25);

    this.dp_.Runtime.enable();
    this.dp_.Runtime.onConsoleAPICalled(data => {
      const text = data.params.args[0].value;
      this.testRunner_.log(text);
    });

    return {httpInterceptor, frameNavigationHelper, virtualTimeController};
  }

});
