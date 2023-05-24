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

BouncingCssShape = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = this._createSpan(stage);

        switch (stage.fill) {
        case "solid":
        default:
            this.element.style.backgroundColor = Stage.randomColor();
            break;

        case "gradient":
            this.element.style.background = "linear-gradient(" + Stage.randomColor() + ", " + Stage.randomColor() + ")";
            break;
        }

        if (stage.blend)
            this.element.style.mixBlendMode = Stage.randomStyleMixBlendMode();

        // Some browsers have not un-prefixed the css filter yet.
        if (stage.filter)
            Utilities.setElementPrefixedProperty(this.element, "filter", Stage.randomStyleFilter());

        this._move();
    }, {

    _createSpan: function(stage)
    {
        var span = document.createElement("span");
        span.className = stage.shape + " " + stage.clip;
        span.style.width = this.size.x + "px";
        span.style.height = this.size.y + "px";
        stage.element.appendChild(span);
        return span;
    },

    _move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px," + this.position.y + "px)" + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this.rotater.next(timeDelta);
        this._move();
    }
});

BouncingCssShapesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.parseShapeParameters(options);
    },

    createParticle: function()
    {
        return new BouncingCssShape(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingCssShapesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCssShapesStage(), options);
    }
);

window.benchmarkClass = BouncingCssShapesBenchmark;

})();
