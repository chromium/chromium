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

BouncingCompositedImage = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("img");
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.setAttribute("src", stage.imageSrc);

        if (stage.useFilters)
            this.element.style.filter = "hue-rotate(" + Stage.randomAngle() + "rad)";

        stage.element.appendChild(this.element);
        this._move();
    }, {

    _move: function()
    {
        this.element.style.transform = "translate3d(" + this.position.x + "px," + this.position.y + "px, 0) " + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

CompositingTransformsStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);

        this.imageSrc = options["imageSrc"] || "../resources/yin-yang.svg";
        this.useFilters = options["filters"] == "yes";
    },

    createParticle: function()
    {
        return new BouncingCompositedImage(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

CompositedTransformsBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new CompositingTransformsStage(), options);
    }
);

window.benchmarkClass = CompositedTransformsBenchmark;

})();
