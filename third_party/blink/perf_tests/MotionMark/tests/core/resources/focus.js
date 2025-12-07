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

var minimumDiameter = 30;
var sizeVariance = 20;
var travelDistance = 50;

var minBlurValue = 1;
var maxBlurValue = 10;

var opacityMultiplier = 30;
var focusDuration = 1000;
var movementDuration = 2500;

var FocusElement = Utilities.createClass(
    function(stage)
    {
        var size = minimumDiameter + sizeVariance;

        // Size and blurring are a function of depth.
        this._depth = Pseudo.random();
        var distance = Utilities.lerp(this._depth, 0, sizeVariance);
        size -= distance;

        var top = Stage.random(0, stage.size.height - size);
        var left = Stage.random(0, stage.size.width - size);

        this.particle = document.createElement("div");
        this.particle.style.width = size + "px";
        this.particle.style.height = size + "px";
        this.particle.style.top = top + "px";
        this.particle.style.left = left + "px";
        this.particle.style.zIndex = Math.round((1 - this._depth) * 10);

        var depthMultiplier = Utilities.lerp(1 - this._depth, 0.8, 1);
        this._sinMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;
        this._cosMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;

        this.animate(stage, 0, 0);
    }, {

    hide: function()
    {
        this.particle.style.display = "none";
    },

    show: function()
    {
        this.particle.style.display = "block";
    },

    animate: function(stage, sinFactor, cosFactor)
    {
        var top = sinFactor * this._sinMultiplier;
        var left = cosFactor * this._cosMultiplier;
        var distance = Math.abs(this._depth - stage.focalPoint);
        var blur = Utilities.lerp(distance, minBlurValue, maxBlurValue);
        var opacity = Math.max(5, opacityMultiplier * (1 - distance));

        Utilities.setElementPrefixedProperty(this.particle, "filter", "blur(" + blur + "px) opacity(" + opacity + "%)");
        this.particle.style.transform = "translate3d(" + left + "%, " + top + "%, 0)";
    }
});

var FocusStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        this._testElements = [];
        this._offsetIndex = 0;
        this.focalPoint = 0.5;
    },

    complexity: function()
    {
        return this._offsetIndex;
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
            this.element.appendChild(obj.particle);
        }
        for (var i = this._offsetIndex; i < newIndex; ++i)
            this._testElements[i].show();
        this._offsetIndex = newIndex;
    },

    animate: function()
    {
        var time = this._benchmark.timestamp;
        var sinFactor = Math.sin(time / movementDuration);
        var cosFactor = Math.cos(time / movementDuration);

        this.focalPoint = 0.5 + 0.5 * Math.sin(time / focusDuration);

        for (var i = 0; i < this._offsetIndex; ++i)
            this._testElements[i].animate(this, sinFactor, cosFactor);
    }
});

var FocusBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new FocusStage(), options);
    }
);

window.benchmarkClass = FocusBenchmark;

}());
