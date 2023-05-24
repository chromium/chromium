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

BouncingCanvasShape = Utilities.createSubclass(BouncingCanvasParticle,
    function(stage)
    {
        BouncingCanvasParticle.call(this, stage, stage.shape);
        this._fill = stage.fill;
        this._color0 = Stage.randomColor();
        this._color1 = Stage.randomColor();
    }, {

    _applyFill: function()
    {
        switch (this._fill) {
        case "gradient":
            var gradient = this.context.createLinearGradient(0, 0, this.size.width, 0);
            gradient.addColorStop(0, this._color0);
            gradient.addColorStop(1, this._color1);
            this.context.fillStyle = gradient;
            break;

        case "solid":
        default:
            this.context.fillStyle = this._color0;
            break;
        }
    },

    _drawShape: function()
    {
        this.context.beginPath();

        switch (this._shape) {
        case "rect":
            this.context.rect(0, 0, this.size.width, this.size.height);
            break;

        case "circle":
        default:
            var center = this.size.center;
            var radius = Math.min(this.size.x, this.size.y) / 2;
            this.context.arc(center.x, center.y, radius, 0, Math.PI * 2, true);
            break;
        }

        this.context.fill();
    },

    _draw: function()
    {
        this.context.save();
            this._applyFill();
            this.applyRotation();
            this.applyClipping();
            this._drawShape();
        this.context.restore();
    }
});

BouncingCanvasShapesStage = Utilities.createSubclass(BouncingCanvasParticlesStage,
    function ()
    {
        BouncingCanvasParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingCanvasParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.parseShapeParameters(options);
    },

    createParticle: function()
    {
        return new BouncingCanvasShape(this);
    }
});

BouncingCanvasShapesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCanvasShapesStage(), options);
    }
);

window.benchmarkClass = BouncingCanvasShapesBenchmark;

})();