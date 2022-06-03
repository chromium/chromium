# IPP Attribute code generation

This directory contains tools to handle IPP attributes based on data provided in
IPP registrations. The authoritative source for CSVs in this directory is the
[registrations page hosted by IANA](https://www.iana.org/assignments/ipp-registrations/ipp-registrations.xhtml)
([direct link to Attributes CSV](https://www.iana.org/assignments/ipp-registrations/ipp-registrations-2.csv),
[direct link to Keyword Values CSV](https://www.iana.org/assignments/ipp-registrations/ipp-registrations-4.csv),
[direct link to Enum Values CSV](https://www.iana.org/assignments/ipp-registrations/ipp-registrations-6.csv)).

If CSVs are updated one should be able to drop in the new version as is but
localizations might have to be added to
[components/printing_component_strings.grdp](components/printing_component_strings.grdp).
TODO: Generate placeholders in grdp.
