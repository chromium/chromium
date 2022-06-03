# Linux GTK Theme Integration

The GTK+ port of Chromium has a mode where we try to match the user's GTK theme
(which can be enabled under Settings -> Appearance -> Use GTK+ theme).

## How Chromium determines which colors to use

GTK3 added a new CSS theming engine which gives fine-tuned control over how
widgets are styled. Chromium's themes, by contrast, are much simpler: it is
mostly a list of about 80 colors (see //src/ui/native_theme/native_theme.h)
overridden by the theme. Chromium usually doesn't use GTK to render entire
widgets, but instead tries to determine colors from them.

Chromium needs foreground, background and border colors from widgets.  The
foreground color is simply taken from the CSS "color" property.  Backgrounds and
borders are complicated because in general they might have multiple gradients or
images. To get the color, Chromium uses GTK to render the background or border
into a 24x24 bitmap and uses the average color for theming. This mostly gives
reasonable results, but in case theme authors do not like the resulting color,
they have the option to theme Chromium widgets specially.

## Note to GTK theme authors: How to theme Chromium widgets

Every widget Chromium uses will have a "chromium" style class added to it. For
example, a textfield selector might look like:

```
.window.background.chromium .entry.chromium
```

If themes want to handle Chromium textfields specially, for GTK3.0 - GTK3.19,
they might use:

```
/* Normal case */
.entry {
    color: #ffffff;
    background-color: #000000;
}

/* Chromium-specific case */
.entry.chromium {
    color: #ff0000;
    background-color: #00ff00;
}
```

For GTK3.20 or later, themes will as usual have to replace ".entry" with
"entry".

The list of CSS selectors that Chromium uses to determine its colors is in
//src/ui/gtk/native_theme_gtk.cc.
