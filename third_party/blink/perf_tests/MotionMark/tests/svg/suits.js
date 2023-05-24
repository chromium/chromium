/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

var SuperSuitsParticle = window.SuitsParticle;
ClipSuit = Utilities.createSubclass(SuperSuitsParticle,
    function(stage)
    {
        this.initialize(stage);
    }, {

    isClipPath: true,
    hasGradient: false,
    move: function()
    {
        this.element.setAttribute("transform", "translate(" + (this.position.x - this.size.center.x) + "," + (this.position.y - this.size.center.x) + ")");
    }
});

ShapeSuit = Utilities.createSubclass(SuperSuitsParticle,
    function(stage)
    {
        this.initialize(stage);
    }, {

    isClipPath: false,
    hasGradient: false,
    move: function()
    {
        this.element.setAttribute("transform", "translate(" + this.position.x + "," + this.position.y + ") " + this.transformSuffix);
    }
});

RotationSuit = Utilities.createSubclass(SuperSuitsParticle,
    function(stage)
    {
        this.isClipPath = stage.particleCounter % 2;
        this.initialize(stage);
    }, {

    hasGradient: false,
});

GradientSuit = Utilities.createSubclass(SuperSuitsParticle,
    function(stage)
    {
        this.isClipPath = stage.particleCounter % 2;
        this.initialize(stage);
    }, {

    hasGradient: true,
    move: function()
    {
        this.element.setAttribute("transform", "translate(" + this.position.x + "," + this.position.y + ") " + this.transformSuffix);
    }
});

StaticSuit = Utilities.createSubclass(SuperSuitsParticle,
    function(stage)
    {
        this.isClipPath = stage.particleCounter % 2;
        this.initialize(stage);
    }, {

    hasGradient: true,
    reset: function()
    {
        SuperSuitsParticle.prototype.reset.call(this);
        this.originalPosition = Stage.randomPosition(this.stage.size);
        this.transformSuffix = " rotate(" + Math.floor(Stage.randomAngle() * 180 / Math.PI) + ",0,0)" + this.transformSuffix;
    },

    move: function()
    {
        this.element.setAttribute("transform", "translate(" + this.originalPosition.x + "," + this.originalPosition.y + ") " + this.transformSuffix);
    }
});

var SuitsBenchmark = window.benchmarkClass;
var SuitsDerivedBenchmark = Utilities.createSubclass(SuitsBenchmark,
    function(options)
    {
        switch (options["style"]) {
        case "clip":
            window.SuitsParticle = ClipSuit;
            break;
        case "shape":
            window.SuitsParticle = ShapeSuit;
            break;
        case "rotation":
            window.SuitsParticle = RotationSuit;
            break;
        case "gradient":
            window.SuitsParticle = GradientSuit;
            break;
        case "static":
            window.SuitsParticle = StaticSuit;
            break;
        }
        SuitsBenchmark.call(this, options);
    }
);

window.benchmarkClass = SuitsDerivedBenchmark;

})();
