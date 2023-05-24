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
BouncingSvgParticle = Utilities.createSubclass(BouncingParticle,
    function(stage, shape)
    {
        BouncingParticle.call(this, stage);
        this._shape = shape;
    }, {

    _applyClipping: function(stage)
    {
        if (stage.clip != "star")
            return;

        stage.ensureClipStarIsCreated();
        this.element.setAttribute("clip-path", "url(#star-clip)");
    },

    _move: function()
    {
        var transform = "translate(" + this.position.x + ", " + this.position.y + ")";
        if (this._shape != "circle")
            transform += this.rotater.rotate(this.size.center);
        this.element.setAttribute("transform", transform);
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

BouncingSvgParticlesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    _createDefs: function()
    {
        return Utilities.createSVGElement("defs", {}, {}, this.element);
    },

    _ensureDefsIsCreated: function()
    {
        return this.element.querySelector("defs") || this._createDefs();
    },

    _createClipStar: function()
    {
        var attrs = { id: "star-clip", clipPathUnits: "objectBoundingBox" };
        var clipPath  = Utilities.createSVGElement("clipPath", attrs, {}, this._ensureDefsIsCreated());

        attrs = { d: "M.50,0L.38,.38L0,.38L.30,.60L.18,1L.50,.75L.82,1L.70,.60L1,.38L.62,.38z" };
        Utilities.createSVGElement("path", attrs, {}, clipPath);
        return clipPath;
    },

    ensureClipStarIsCreated: function()
    {
        return this.element.querySelector("#star-clip") || this._createClipStar();
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});
