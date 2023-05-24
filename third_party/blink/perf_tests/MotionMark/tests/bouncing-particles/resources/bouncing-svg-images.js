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

BouncingSvgImage = Utilities.createSubclass(BouncingSvgParticle,
    function(stage)
    {
        BouncingSvgParticle.call(this, stage, "image");

        var attrs = { x: 0, y: 0, width: this.size.x, height: this.size.y };
        var xlinkAttrs = { href: stage.imageSrc };
        this.element = Utilities.createSVGElement("image", attrs, xlinkAttrs, stage.element);
        this._move();
    }
);

BouncingSvgImagesStage = Utilities.createSubclass(BouncingSvgParticlesStage,
    function()
    {
        BouncingSvgParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingSvgParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
    },

    createParticle: function()
    {
        return new BouncingSvgImage(this);
    }
});

BouncingSvgImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingSvgImagesStage(), options);
    }
);

window.benchmarkClass = BouncingSvgImagesBenchmark;

})();

