/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
(function() {

var maxVerticalOffset = 50;
var minimumDiameter = 30;
var centerDiameter = 90;
var sizeVariance = 60;
var travelDistance = 50;

var opacityMultiplier = 30;

var FocusElement = Utilities.createClass(
    function(stage)
    {
        var size = minimumDiameter + sizeVariance;

        // Size and blurring are a function of depth.
        this._depth = Pseudo.random();
        var distance = Utilities.lerp(this._depth, 0, sizeVariance);
        size -= distance;

        var top = Stage.random(0, stage.size.height - size) - stage.maxBlurValue * 3;
        var left = Stage.random(0, stage.size.width - size) - stage.maxBlurValue * 3;

        this.container = document.createElement('div');
        this.container.style.width = (size + stage.maxBlurValue * 6) + "px";
        this.container.style.height = (size + stage.maxBlurValue * 6) + "px";
        this.container.style.top = top + "px";
        this.container.style.left = left + "px";
        this.container.style.zIndex = Math.round((1 - this._depth) * 10);

        this.particle = Utilities.createElement("div", {}, this.container);
        this.particle.style.width = size + "px";
        this.particle.style.height = size + "px";
        this.particle.style.top = (stage.maxBlurValue * 3) + "px";
        this.particle.style.left = (stage.maxBlurValue * 3) + "px";

        var depthMultiplier = Utilities.lerp(1 - this._depth, 0.8, 1);
        this._sinMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;
        this._cosMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;
    }, {

    hide: function()
    {
        this.container.style.display = "none";
    },

    show: function()
    {
        this.container.style.display = "block";
    },

    animate: function(stage, sinFactor, cosFactor)
    {
        var top = sinFactor * this._sinMultiplier;
        var left = cosFactor * this._cosMultiplier;

        Utilities.setElementPrefixedProperty(this.container, "filter", "blur(" + stage.getBlurValue(this._depth) + "px) opacity(" + stage.getOpacityValue(this._depth) + "%)");
        this.container.style.transform = "translate3d(" + left + "%, " + top + "%, 0)";
    }
});

var FocusStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    movementDuration: 2500,
    focusDuration: 1000,

    centerObjectDepth: 0.0,

    minBlurValue: 1.5,
    maxBlurValue: 15,
    maxCenterObjectBlurValue: 5,

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        this._testElements = [];
        this._focalPoint = 0.5;
        this._offsetIndex = 0;

        this._centerElement = document.getElementById("center-text");
        this._centerElement.style.width = (centerDiameter + this.maxCenterObjectBlurValue * 6) + "px";
        this._centerElement.style.height = (centerDiameter + this.maxCenterObjectBlurValue * 6) + "px";
        this._centerElement.style.zIndex = Math.round(10 * this.centerObjectDepth);

        var particle = document.querySelector("#center-text div");
        particle.style.width = centerDiameter + "px";
        particle.style.height = centerDiameter + "px";
        particle.style.top = (this.maxCenterObjectBlurValue * 3) + "px";
        particle.style.left = (this.maxCenterObjectBlurValue * 3) + "px";

        var blur = this.getBlurValue(this.centerObjectDepth, true);
        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + blur + "px)");
    },

    complexity: function()
    {
        return 1 + this._offsetIndex;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(0, this._offsetIndex + count);
            for (var i = this._offsetIndex; i < this._testElements.length; ++i)
                this._testElements[i].hide();
            return;
        }

        var newIndex = this._offsetIndex + count;
        for (var i = this._testElements.length; i < newIndex; ++i) {
            var obj = new FocusElement(this);
            this._testElements.push(obj);
            this.element.appendChild(obj.container);
        }
        for (var i = this._offsetIndex; i < newIndex; ++i)
            this._testElements[i].show();
        this._offsetIndex = newIndex;
    },

    animate: function()
    {
        var time = this._benchmark.timestamp;
        var sinFactor = Math.sin(time / this.movementDuration);
        var cosFactor = Math.cos(time / this.movementDuration);

        var focusProgress = 0.5 + 0.5 * Math.sin(time / this.focusDuration);
        this._focalPoint = focusProgress;

        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + this.getBlurValue(this.centerObjectDepth, true) + "px)");

        for (var i = 0; i < this._offsetIndex; ++i)
            this._testElements[i].animate(this, sinFactor, cosFactor);
    },

    getBlurValue: function(depth, isCenter)
    {
        if (isCenter)
            return 1 + Math.abs(depth - this._focalPoint) * (this.maxCenterObjectBlurValue - 1);

        return Utilities.lerp(Math.abs(depth - this._focalPoint), this.minBlurValue, this.maxBlurValue);
    },

    getOpacityValue: function(depth)
    {
        return Math.max(1, opacityMultiplier * (1 - Math.abs(depth - this._focalPoint)));
    },
});

var FocusBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new FocusStage(), options);
    }
);

window.benchmarkClass = FocusBenchmark;

}());
